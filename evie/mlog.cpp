/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mlog.hpp"
#include "mutex.hpp"
#include "mtime.hpp"
#include "utils.hpp"
#include "fast_alloc.hpp"

#include <fstream>
#include <thread>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

mtime_parsed parse_mtime_impl(const mtime& time)
{
    //very slow function
    mtime_parsed ret;
    struct tm * t = gmtime(&time.t.tv_sec);
    ret.year = t->tm_year + 1900;
    ret.month = t->tm_mon;
    ret.day = t->tm_mday;
    ret.hours = t->tm_hour;
    ret.minutes = t->tm_min;
    ret.seconds = t->tm_sec;
    ret.nanos = time.nanos();
    return ret;
}
const uint32_t cur_day_seconds = get_cur_mtime().day_seconds(); //first call of get_cur_mtime() can be slow
const mtime_date cur_day_date = parse_mtime_impl(get_cur_mtime_seconds());
inline my_string get_cur_day_str()
{
    std::stringstream str;
    str << mlog_fixed<4>(cur_day_date.year) << "-" << mlog_fixed<2>(cur_day_date.month) << "-" << mlog_fixed<2>(cur_day_date.day);
    std::string s = str.str();
    return my_string(&s[0], s.size());
}

const my_string cur_day_date_str = get_cur_day_str();

void parse_mtime(const mtime& time, mtime_parsed& ret)
{
    if(time.day_seconds() == cur_day_seconds){
        static_cast<mtime_date&>(ret) = cur_day_date;
        uint32_t frac = time.total_sec() % (24 * 3600);
        ret.seconds = frac % 60;
        ret.hours = frac / 3600;
        ret.minutes = (frac - ret.hours * 3600) / 60;
        ret.nanos = time.nanos();
    }
    else
        ret = parse_mtime_impl(time);
}
mtime_time_duration get_mtime_time_duration(const mtime& time)
{
    mtime_time_duration ret;
    uint32_t frac = time.total_sec() % (24 * 3600);
    ret.seconds = frac % 60;
    ret.hours = frac / 3600;
    ret.minutes = (frac - ret.hours * 3600) / 60;
    ret.nanos = time.nanos();
    return ret;
}
namespace
{
    static const uint32_t buf_size = 1000;
    static const uint32_t pre_alloc = 1024;
    class my_pool
    {
        lockfree_queue<void*, pre_alloc> queue;

    public:
        my_pool() {
            for(uint32_t i = 0; i != pre_alloc; ++i)
                queue.push(::malloc(buf_size + 1));
        }
        void* malloc() {
            void* p;
            if(!queue.pop(p))
                p = ::malloc(buf_size + 1);
            return p;
        }
        void free(void* p) {
            while(!queue.push(p)) {
            }
        }
        ~my_pool() {
            void* p;
            while(queue.pop(p))
                ::free(p);
        }
    };
    class ofile
    {
        std::string name;
        int hfile;
    public:
        ofile(const char* file) : name(file)
        {
            hfile = ::open(file, O_WRONLY | O_CREAT | O_APPEND
                , S_IWRITE | S_IREAD | S_IREAD | S_IWRITE | S_IRGRP | S_IWGRP);
            if(hfile < 0)
                throw_system_failure(es() % "open file " % name % " error");
        }
        void lock()
        {
            struct flock lock_struct;
            lock_struct.l_type = F_WRLCK | F_RDLCK;
            lock_struct.l_whence = SEEK_SET;
            lock_struct.l_start = 0;
            lock_struct.l_len = 0;
            if((::fcntl(hfile, F_SETLK, &lock_struct)) < 0)
                throw_system_failure(es() % "lock file " % name % " error");
        }
        void write(const char* buf, uint32_t count)
        {
            if(::write(hfile, buf, count) != count)
                throw_system_failure(es() % "file " % name % " writing error");
        }
        ~ofile()
        {
            ::close(hfile);
        }
    };
}

class simple_log
{
    std::unique_ptr<ofile> stream, stream_crit;
    long params;
    volatile bool can_run;
    std::thread work_thread;
    struct Data{
        char* buf;
        uint32_t size;
        long extra_param;
        Data() : buf(), size(), extra_param(){
        }
        Data(char* buf, uint32_t size, long extra_param) : buf(buf), size(size), extra_param(extra_param){
        }
    };

    typedef lockfree_queue<Data, pre_alloc> Strs;
    Strs strs;
    std::atomic<uint32_t> all_size;

    static const uint32_t log_no_cout_size = 10;
    void write_impl(char* str, uint32_t size, long extra_param, uint32_t all_sz, uint32_t cur_size){
        if(str){
            if(!no_cout && extra_param == mlog::only_cout){
                std::cout.write(str, size);
                std::cout.flush();
            }
            else{
                if(stream)
                    (*stream).write(str, size);
                if((extra_param & mlog::critical) && stream_crit)
                    (*stream_crit).write(str, size);
                if((!stream || ((params & mlog::always_cout) && !(extra_param & mlog::no_cout))) && !no_cout){
                    if(all_sz >= log_no_cout_size){
                        if(!cur_size)
                            std::cout << "[[skipped " << all_sz << " rows from logging to stdout, see log file for found it]]" << std::endl;
                    }
                    else{
                        std::cout.write(str, size);
                    }
                }
            }
        }
    }
    void WriteThred(){
        set_trash_thread();
        uint32_t writed_sz = 0;
        try{
            Data data;
            static const uint32_t cntr_from = 128, cntr_to = 524288;
            uint32_t cntr = 128;
            for(;;){
                uint32_t all_sz = all_size - writed_sz;
                for(uint32_t cur_i = 0; cur_i != all_sz; ++cur_i){
                    if(!strs.pop(data))
                        throw std::runtime_error("strs::pop item not selected");
                    write_impl(data.buf, data.size, data.extra_param, all_sz, cur_i);
                    ++writed_sz;
                    pool.free(data.buf);
                }
                if(all_sz){
                    if(cntr != cntr_from)
                        cntr /= 2;
                }
                if(!all_sz && !can_run)
                    break;
                if(!all_sz){
                    usleep(cntr);
                    if(cntr != cntr_to)
                        cntr *= 2;
                }
            }
        }
        catch(std::exception& e){
            std::cout << "simple_log::WriteThread error: " << e.what() << std::endl;
        }
    }
public:
    volatile bool no_cout;
    long& Params() {
        return params;
    }
    simple_log() : can_run(true), work_thread(&simple_log::WriteThred, this),
        all_size(), no_cout()
    {
    }
    my_pool pool;
    ~simple_log(){
        can_run = false;
        work_thread.join();
    }
    void init(const char* file_name, long params){
        this->params = params;
        if(file_name){
            stream.reset(new ofile(file_name));
            if(!(params & mlog::no_crit_file)){
                std::string crit_file = std::string(file_name) + "_crit";
                stream_crit.reset(new ofile(crit_file.c_str()));
            }
            if(params & mlog::lock_file){
                stream->lock();
                if(stream_crit)
                    stream_crit->lock();
            }
        }
        else{
            stream.reset();
            stream_crit.reset();
        }
    }
    static simple_log* my_log;
    static void set_instance(simple_log* log){
        my_log = log;
    };
    static simple_log& instance(){
        return *my_log;
    }
    long Params() const{
        return params;
    }
    void WriteBulk(const std::vector<std::pair<char*, uint32_t> >& blocks, long extra_param){
        std::vector<std::pair<char*, uint32_t> >::const_iterator it = blocks.begin(), it_e = blocks.end();
        for(; it != it_e; ++it)
            Write(it->first, it->second, extra_param);
    }
    void Write(char* buf, uint32_t size, long extra_param){
        strs.push(Data(buf, size, extra_param));
        ++all_size;
    }
};

void mlog::set_no_cout()
{
    simple_log::instance().no_cout = true;
}

simple_log* log_init(const char* file_name, long params)
{
    static simple_log log;
    simple_log::set_instance(&log);
    simple_log::instance().init(file_name, params);
    return &log;
}

simple_log* log_create(const char* file_name, long params)
{
    std::unique_ptr<simple_log> log(new simple_log());
    log->init(file_name, params);
    return log.release();
}

void log_destroy(simple_log* log)
{
    delete log;
}

void log_set(simple_log* log)
{
    simple_log::set_instance(log);
}

simple_log* log_get()
{
    return &simple_log::instance();
}

long& log_params()
{
    return log_get()->Params();
}

std::atomic<uint32_t> thread_tss_id;
static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
static void make_key(){
    pthread_key_create(&key, NULL);
}

static int get_thread_id()
{
    void *ptr;
    pthread_once(&key_once, make_key);
    if((ptr = pthread_getspecific(key)) == NULL)
    {
        ptr = new int(++thread_tss_id);
        pthread_setspecific(key, ptr);
    }
    return *((int*)ptr);
}

void mlog::init()
{
    {
        //MPROFILE("Pool_alloc")
        buf = (char*)log.pool.malloc();
    }
    if(extra_param == only_cout)
        return;

    long params = log.Params() | extra_param;

    if(params & mlog::store_pid){
        (*this) << "pid: " << getpid() << " ";
    }
    if(params & mlog::store_tid){
        (*this) << "tid: " << get_thread_id() << " ";       
    }
    //MPROFILE("Pool_tail")
    if(extra_param & mlog::warning)
        (*this) << "WARNING ";
    if(extra_param & mlog::error)
        (*this) << "ERROR ";
    (*this) << get_cur_mtime() << ": ";
}

mlog::mlog(long extra_param) : log(simple_log::instance()), extra_param(extra_param), cur_size(), blocks(NULL)
{
    init();
}

mlog::mlog(simple_log* log, long extra_param) : log(*log), extra_param(extra_param), cur_size(), blocks(NULL)
{
    init();
}

mlog::~mlog()
{
    if(extra_param != only_cout)
        (*this) << std::endl;
    if(blocks){
        blocks->push_back(std::pair<char*, uint32_t>(buf, cur_size));
        log.WriteBulk(*blocks, extra_param);
        delete blocks;
    }
    else
        log.Write(&buf[0], cur_size, extra_param);
}

mlog& mlog::operator<<(char s)
{
    check_size(1);
    buf[cur_size++] = s;
    return *this;
}

void mlog::write_string(const char* it, uint32_t size)
{
    while(size) {
        uint32_t cur_write = std::min(size, buf_size - cur_size);
        my_fast_copy(it, cur_write, &buf[cur_size]);
        cur_size += cur_write;
        it += cur_write;
        size -= cur_write;
        if(cur_size == buf_size) {
            if(!blocks)
                blocks = new std::vector<std::pair<char*, uint32_t> >(1, std::pair<char*, uint32_t>(buf, cur_size));
            else
                blocks->push_back(std::pair<char*, uint32_t>(buf, cur_size));
            cur_size = 0;
            buf = 0;
            buf = (char*)log.pool.malloc();
        }
    }
}

mlog& mlog::operator<<(const std::string& s)
{
    uint32_t sz = s.size();
    write_string(&s[0], sz);
    return *this;
}

mlog& mlog::operator<<(std::ostream& (std::ostream&))
{
    check_size(1);
    buf[cur_size++] = '\n';
    return *this;
}

mlog& mlog::operator<<(const mtime_date& d)
{
    if(d == cur_day_date)
        (*this) << cur_day_date_str;
    else
        (*this) << d.year << '-' << print2chars(d.month) << '-' << print2chars(d.day);
    return *this;
}

mlog& mlog::operator<<(const mtime_time_duration& t)
{
    //MPROFILE("Pool_time_durat")
    (*this) << print2chars(t.hours) << ':' << print2chars(t.minutes) << ':' << print2chars(t.seconds);
    (*this) << "." << mlog_fixed<6>(t.nanos / 1000);
    return *this;
}

mlog& mlog::operator<<(const mtime_parsed& p)
{
    (*this) << static_cast<const mtime_date&>(p) << ' ' << static_cast<const mtime_time_duration&>(p);
    return *this;
}

mlog& mlog::operator<<(const mtime& p)
{
    mtime_parsed mp;
    parse_mtime(p, mp);
    (*this) << mp;
    return *this;
}

mlog& mlog::operator<<(const std::exception& e)
{
    extra_param |= critical;
    return (*this) << "exception: " << str_holder(e.what());
}

void mlog::check_size(uint32_t delta)
{
    if(cur_size + delta > buf_size){
        if(delta > buf_size)
            throw std::runtime_error("mlog() max size exceed");
        if(!blocks)
            blocks = new std::vector<std::pair<char*, uint32_t> >(1, std::pair<char*, uint32_t>(buf, cur_size));
        else
            blocks->push_back(std::pair<char*, uint32_t>(buf, cur_size));
        cur_size = 0;
        buf = 0;
        buf = (char*)log.pool.malloc();
    }
}

void MlogTestThread(size_t thread_id, size_t log_count)
{
    mlog() << "thread " << thread_id << " started";
    MPROFILE("MlogThread")
    for(size_t i = 0; i != log_count; ++i){
        //MPROFILE("MlogLoop")
        //MPROFILE_THREAD("MlogLoop_thread")
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
    simple_log::instance().Params() = mlog::always_cout;
    mlog() << "mlog test for " << thread_count << " threads and " << log_count << " loops";
    MPROFILE("MlogTest");
    {
        std::vector<std::thread> threads;
        for(size_t i = 0; i != thread_count; ++i)
            threads.push_back(std::thread(&MlogTestThread, i, log_count));
		for(auto&& t: threads)
            t.join();
    }
    mlog() << "mlog test successfully ended";
}

simple_log* simple_log::my_log = 0;
static const uint32_t trash_t1 = 3, trash_t2 = 9;
void set_affinity_thread(uint32_t thrd)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thrd, &cpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
        mlog(mlog::critical) << "pthread_setaffinity_np() error";
}
void set_trash_thread(){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(trash_t1, &cpuset);
    CPU_SET(trash_t2, &cpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
        mlog(mlog::critical) << "pthread_setaffinity_np() error";
}
void set_significant_thread(){
    return;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for(uint32_t i = 0; i != 11; ++i){
        if(i != trash_t1 && i != trash_t2)
            CPU_SET(i, &cpuset);
    }
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
        mlog(mlog::critical) << "pthread_setaffinity_np() error";
}

void log_start_params(int argc, char** argv)
{
    mlog ml(mlog::no_cout);
    for(int i = 0; i != argc; ++i)
        ml << str_holder(argv[i]) << " ";
}

const char binary16[] = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F202122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F404142434445464748494A4B4C4D4E4F505152535455565758595A5B5C5D5E5F606162636465666768696A6B6C6D6E6F707172737475767778797A7B7C7D7E7F808182838485868788898A8B8C8D8E8F909192939495969798999A9B9C9D9E9FA0A1A2A3A4A5A6A7A8A9AAABACADAEAFB0B1B2B3B4B5B6B7B8B9BABBBCBDBEBFC0C1C2C3C4C5C6C7C8C9CACBCCCDCECFD0D1D2D3D4D5D6D7D8D9DADBDCDDDEDFE0E1E2E3E4E5E6E7E8E9EAEBECEDEEEFF0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF";
str_holder itoa_hex(uint8_t ch){
    return str_holder(&binary16[ch * 2], 2);
}

