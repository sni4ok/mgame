/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "utils.hpp"
#include "thread.hpp"
#include "fast_alloc.hpp"

#include "stdio.h"

#include <fcntl.h>

#include <sys/stat.h>
#include <sys/file.h>

namespace
{
    class ofile
    {
        mstring name;
        int hfile;

    public:
        ofile(const char* file) : name(_str_holder(file))
        {
            hfile = ::open(file, O_WRONLY | O_CREAT | O_APPEND, S_IWRITE | S_IREAD | S_IRGRP | S_IWGRP);
            if(hfile <= 0)
                throw_system_failure(es() % "open file " % name % " error");
        }
        void lock()
        {
            if(flock(hfile, LOCK_EX | LOCK_NB))
                throw_system_failure(es() % "lock file " % name % " error");
        }
        void write(const char* buf, uint32_t size)
        {
            if(::write(hfile, buf, size) != size)
                throw_system_failure(es() % "file " % name % " writing error");
        }
        ~ofile()
        {
            ::close(hfile);
        }
    };
}

void cout_write(str_holder str, bool flush)
{
    fwrite(str.str, 1, str.size, stdout);
    if(flush)
        fflush(stdout);
}

void cerr_write(str_holder str, bool flush)
{
    fwrite(str.str, 1, str.size, stderr);
    if(flush)
        fflush(stderr);
}

class simple_log
{
    static const uint32_t log_no_cout_size = 10;

    typedef fast_alloc<mlog::node> string_pool;
    typedef fast_alloc_list<mlog::data> nodes_type;

    void write_impl(char* str, uint32_t size, uint32_t extra_param, uint32_t all_sz, uint32_t cur_size)
    {
        if(str)
        {
            if(stream)
                stream->write(str, size);
            if((extra_param & mlog::critical) && stream_crit)
                stream_crit->write(str, size);
            if((!stream || ((params & mlog::always_cout) && !(extra_param & mlog::no_cout))) && !no_cout)
            {
                if(all_sz >= log_no_cout_size)
                {
                    if(!cur_size)
                        cout_write(es() % "[[skipped " % all_sz % " rows from logging to stdout, see log file for found it]]" % endl, true);
                }
                else
                    cout_write({str, size}, false);
            }
        }
    }
    void write_thred()
    {
        set_trash_thread();
        uint32_t writed_sz = 0;
        try
        {
            mlog::data* data;
            static const uint32_t cntr_from = 128, cntr_to = 524288;
            uint32_t cntr = 128;
            for(;;)
            {
                uint32_t all_sz = atomic_load(all_size) - writed_sz;
                for(uint32_t cur_i = 0; cur_i != all_sz; ++cur_i)
                {
                    data = nodes.pop();

                    while(data->head)
                    {
                        mlog::node* next = data->head->next;
                        write_impl(data->head->buf, data->head->size, data->extra_param, all_sz, cur_i);
                        pool.free(data->head);
                        data->head = next;
                    }

                    nodes.free(data);
                    ++writed_sz;
                }
                if(all_sz)
                {
                    if(cntr != cntr_from)
                        cntr /= 2;
                }
                if(!all_sz && !can_run)
                    break;
                if(!all_sz)
                {
                    pool.run_once();
                    usleep(cntr);
                    if(cntr != cntr_to)
                        cntr *= 2;
                }
            }
        }
        catch(std::exception& e)
        {
            cout_write(es() % "simple_log::write_thread error: " % _str_holder(e.what()) % endl);
        }
    }

    unique_ptr<ofile> stream, stream_crit;
    volatile bool can_run;
    uint32_t all_size;
    string_pool pool;
    nodes_type nodes;
    jthread work_thread;
    static simple_log* log;

    void* free_threads;
    profilerinfo* profiler;

public:
    volatile bool no_cout;
    uint32_t params;

    simple_log(const char* file_name, uint32_t params, bool set_instance)
        : can_run(true), all_size(), nodes("mlog::nodes"), free_threads(), profiler(), no_cout(), params(params)
    {
        if(set_instance)
        {
            log = this;
            free_threads = init_free_threads();
            profiler = new profilerinfo;
        }
        if(file_name)
        {
            stream.reset(new ofile(file_name));
            if(!(params & mlog::no_crit_file))
            {
                mstring crit_file = _mstring(file_name) + "_crit";
                stream_crit.reset(new ofile(crit_file.c_str()));
            }
            if(params & mlog::lock_file)
            {
                stream->lock();
                if(stream_crit)
                    stream_crit->lock();
            }
        }
        work_thread = jthread(&simple_log::write_thred, this);
    }
    ~simple_log()
    {
        can_run = false;
        delete profiler;
        work_thread.join();
        delete_free_threads(free_threads);
    }
    mlog::node* alloc()
    {
        mlog::node* node = pool.alloc();
        node->next = nullptr;
        node->size = 0;
        return node;
    }
    static void set_instance(simple_log* l)
    {
        log = l;
        set_free_threads(l->free_threads);
        profilerinfo::set_instance(l->profiler);
    };
    static simple_log& instance()
    {
        return *log;
    }
    void write(const mlog::data& buf)
    {
        nodes.emplace(buf);
        atomic_add(all_size, 1);
    }
};

void mlog::set_no_cout()
{
    simple_log::instance().no_cout = true;
}

void simple_log_free(simple_log* ptr)
{
    delete ptr;
}

unique_ptr<simple_log, simple_log_free> log_init(const char* file_name, uint32_t params, bool set_instance)
{
    return new simple_log(file_name, params, set_instance);
}

void log_set(simple_log* log)
{
    simple_log::set_instance(log);
}

simple_log* log_get()
{
    return &simple_log::instance();
}

uint32_t& log_params()
{
    return log_get()->params;
}

void mlog::init()
{
    buf.head = log.alloc();
    buf.tail = buf.head;

    uint32_t params = log.params | buf.extra_param;
    if(params & mlog::store_pid)
        *this << "pid: " << getpid() << " ";
    if(params & mlog::store_tid)
        *this << "tid: " << get_thread_id() << " ";
    if(buf.extra_param & mlog::warning)
        *this << "WARNING ";
    if(buf.extra_param & mlog::error)
        *this << "ERROR ";
    *this << cur_mtime() << ": ";
}

mlog::mlog(uint32_t extra_param) : log(simple_log::instance())
{
    buf.extra_param = extra_param;
    init();
}

mlog::mlog(simple_log* log, uint32_t extra_param) : log(*log)
{
    buf.extra_param = extra_param;
    init();
}

mlog::~mlog()
{
    *this << '\n';
    log.write(buf);
}

mlog& mlog::operator<<(char s)
{
    check_size(1);
    buf.tail->buf[(buf.tail->size)++] = s;
    return *this;
}

void mlog::write_string(const char* it, uint32_t size)
{
    while(size)
    {
        uint32_t cur_write = min(size, buf_size - buf.tail->size);
        my_fast_copy(it, cur_write, &buf.tail->buf[buf.tail->size]);
        buf.tail->size += cur_write;
        it += cur_write;
        size -= cur_write;
        if(buf.tail->size == buf_size)
        {
            node* tail = buf.tail;
            buf.tail = log.alloc();
            tail->next = buf.tail;
        }
    }
}

void mlog::write(const char* it, uint32_t size)
{
    write_string(it, size);
}

mlog& mlog::operator<<(const time_duration& t)
{
    *this << print2chars(t.hours) << ':' << print2chars(t.minutes) << ':' << print2chars(t.seconds)
        << "." << mlog_fixed<6>(t.nanos / 1000);
    return *this;
}

mlog& mlog::operator<<(const time_parsed& p)
{
    *this << p.date() << ' ' << p.duration();
    return *this;
}

mlog& mlog::operator<<(const ttime_t& p)
{
    *this << parse_time(p);
    return *this;
}

mlog& mlog::operator<<(const std::exception& e)
{
    buf.extra_param |= critical;
    *this << "exception: " << _str_holder(e.what());
    return *this;
}

void mlog::check_size(uint32_t delta)
{
    if(buf.tail->size + delta > buf_size)
    {
        if(delta > buf_size)
            throw mexception("mlog() max size exceed");
        node* tail = buf.tail;
        buf.tail = log.alloc();
        tail->next = buf.tail;
    }
}

mlog& mlog::operator<<(double d)
{
    check_size(30);
    buf.tail->size += my_cvt::dtoa(&buf.tail->buf[buf.tail->size], d);
    return *this;
}

void MlogTestThread(size_t thread_id, size_t log_count)
{
    mlog() << "thread " << thread_id << " started";
    MPROFILE("MlogThread")
    for(size_t i = 0; i != log_count; ++i)
    {
        mlog() << "MlogTestThread_" << thread_id << ", loop:" << i;
    }
}

namespace my_cvt
{
    void test_itoa();
}

void log_test(size_t thread_count, size_t log_count)
{
    my_cvt::test_itoa();
    check_limits();
    simple_log::instance().params = mlog::always_cout;
    mlog() << "mlog test for " << thread_count << " threads and " << log_count << " loops";
    MPROFILE("MlogTest")
    {
        mvector<jthread> threads;
        for(size_t i = 0; i != thread_count; ++i)
            threads.push_back(thread(&MlogTestThread, i, log_count));
    }
    mlog() << "mlog test successfully ended";
}

simple_log* simple_log::log = 0;

const char binary16[] = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F606162636465666768696A6B6C6D6E6F707172737475767778797A7B7C7D7E7F808182838485868788898A8B8C8D8E8F909192939495969798999A9B9C9D9E9FA0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBFC0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2E3E4E5E6E7E8E9EAEBECEDEEEFF0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";

str_holder itoa_hex(uint8_t ch)
{
    return str_holder(&binary16[ch * 2], 2);
}

