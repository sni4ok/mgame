/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mlog.hpp"
#include "utils.hpp"
#include "thread.hpp"
#include "fast_alloc.hpp"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/file.h>

void cout_write(str_holder str, bool flush)
{
    fwrite(str.begin(), 1, str.size(), stdout);
    if(flush)
        fflush(stdout);
}

void cerr_write(str_holder str, bool flush)
{
    fwrite(str.begin(), 1, str.size(), stderr);
    if(flush)
        fflush(stderr);
}

class simple_log
{
    class ofile
    {
        mstring name;
        int hfile;
        static const uint32_t b_size = 1024 * 1024;
        buf_stream_fixed<b_size> b_stream;

    public:
        ofile(char_cit file, bool trunc = false) : name(_str_holder(file))
        {
            int flags = O_WRONLY | O_CREAT | O_APPEND;
            if(trunc)
                flags |= O_TRUNC;
            hfile = ::open(file, flags, S_IWRITE | S_IREAD | S_IRGRP | S_IWGRP);
            if(hfile <= 0)
                throw_system_failure(es() % "open file " % name % " error");
        }
        void lock()
        {
            if(flock(hfile, LOCK_EX | LOCK_NB))
                throw_system_failure(es() % "lock file " % name % " error");
        }
        void flush(bool force)
        {
            if(!b_stream.empty() && (force || (b_stream.size() > b_size / 2)))
            {
                if(uint64_t(::write(hfile, b_stream.begin(), b_stream.size())) != b_stream.size())
                    throw_system_failure(es() % "file " % name % " writing error");
                b_stream.clear();
            }
        }
        void write(char_cit buf, uint32_t size)
        {
            b_stream.write(buf, size);
            flush(false);
        }
        ~ofile()
        {
            flush(true);
            ::close(hfile);
        }
    };

    static const uint32_t log_no_cout_size = 10;

    typedef fast_alloc<mlog::node, mt> string_pool;
    typedef fast_alloc_list<mlog::data, mt> nodes_type;

    void write_impl(char_cit str, uint32_t size, uint32_t params, uint32_t all_sz, bool first)
    {
        if(str)
        {
            if(stream)
                stream->write(str, size);
            if((params & mlog::critical) && stream_crit)
                stream_crit->write(str, size);
            if((!stream || ((params & mlog::always_cout) && !(params & mlog::no_cout))) && !no_cout)
            {
                if(all_sz >= log_no_cout_size)
                {
                    if(first)
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
            my_stream str;
            for(;;)
            {
                uint32_t all_sz = atomic_load(all_size) - writed_sz;
                for(uint32_t cur_i = 0; cur_i != all_sz; ++cur_i)
                {
                    data = nodes.pop();
                    {
                        str.clear();
                        if(data->params & mlog::store_pid)
                            str << "pid: " << data->pid << " ";
                        if(data->params & mlog::store_tid)
                        {
                            uint32_t tid = data->tid;
                            str << "tid: ";
                            if(tid < 10)
                                str << ' ';
                            str << tid << " ";
                        }
                        if(data->params & mlog::warning)
                            str << "WARNING ";
                        if(data->params & mlog::error)
                            str << "ERROR ";
                        str << data->time << ": ";
                        write_impl(str.begin(), str.size(), data->params, all_sz, !cur_i);
                    }

                    while(data->head)
                    {
                        mlog::node* next = data->head->next;
                        write_impl(data->head->buf, data->head->size, data->params, all_sz, false);
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
                    if(stream)
                        stream->flush(true);
                    if(stream_crit)
                        stream_crit->flush(true);
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
        catch(exception& e)
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

    simple_log(char_cit file_name, uint32_t params, bool set_instance)
        : can_run(true), all_size(), nodes("mlog::nodes"), free_threads(), profiler(), no_cout(), params(params)
    {
        if(set_instance)
        {
            log = this;
            free_threads = init_free_threads();
            profiler = new profilerinfo;
            profiler->log_on_exit = false;
        }
        if(file_name)
        {
            stream.reset(new ofile(file_name, params & mlog::truncate_file));
            if(!(params & mlog::no_crit_file))
            {
                mstring crit_file = _str_holder(file_name) + "_crit";
                stream_crit.reset(new ofile(crit_file.c_str(), params & mlog::truncate_crit_file));
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
        profiler->print(mlog::info);
        can_run = false;
        work_thread.join();
        delete profiler;
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

simple_log* simple_log::log = 0;

void mlog::init()
{
    buf.head = log.alloc();
    buf.tail = buf.head;

    buf.time = cur_mtime();
    if(buf.params & mlog::store_pid)
        buf.pid = getpid();
    if(buf.params & mlog::store_tid)
        buf.tid = get_thread_id();
}

void mlog::check_size(uint32_t delta)
{
    if(buf.tail->size + delta > buf_size)
    {
        if(delta > buf_size) [[unlikely]]
            throw str_exception("mlog() max size exceed");
        node* tail = buf.tail;
        buf.tail = log.alloc();
        tail->next = buf.tail;
    }
}

mlog::mlog(uint32_t extra_param) : log(simple_log::instance())
{
    buf.params = log.params | extra_param;
    init();
}

mlog::mlog(simple_log* log, uint32_t extra_param) : log(*log)
{
    buf.params = log->params | extra_param;
    init();
}

mlog::~mlog()
{
    *this << '\n';
    log.write(buf);
}

void mlog::write(char_cit it, uint32_t size)
{
    while(size)
    {
        uint32_t cur_write = min(size, buf_size - buf.tail->size);
        memcpy(&buf.tail->buf[buf.tail->size], it, cur_write);
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

void mlog::set_no_cout()
{
    simple_log::instance().no_cout = true;
}

void free_simple_log(simple_log* ptr)
{
    delete ptr;
}

unique_ptr<simple_log, free_simple_log> log_init(char_cit file_name, uint32_t params, bool set_instance)
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

