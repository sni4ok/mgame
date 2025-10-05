/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "thread.hpp"
#include "signals.hpp"
#include "atomic.hpp"
#include "mlog.hpp"
#include "smart_ptr.hpp"
#include "profiler.hpp"
#include "queue.hpp"

#include <csignal>

#include <pthread.h>
#include <errno.h>

#include <sys/resource.h>

mutex::mutex()
{
    if(pthread_mutex_init(&__mutex, nullptr))
        throw str_exception("create mutex error");
}

mutex::~mutex()
{
    if(pthread_mutex_destroy(&__mutex))
    {
        assert(false && "mutex::~mutex()");
        mlog(mlog::critical) << "mutex::~mutex() pthread_mutex_destroy()";
    }
}

void mutex::lock()
{
    //MPROFILE("mutex::lock")
    if(pthread_mutex_lock(&__mutex))
        throw str_exception("lock mutex error");
}

bool mutex::try_lock()
{
    //MPROFILE("mutex::try_lock")
    if(pthread_mutex_trylock(&__mutex))
        return false;
    return true;
}

void mutex::unlock()
{
    if(pthread_mutex_unlock(&__mutex))
        throw str_exception("unlock mutex error");
}

mutex::scoped_lock::scoped_lock(mutex& mutex) : __mutex(mutex)
{
    lock();
    locked = true;
}

void mutex::scoped_lock::lock()
{
    __mutex.lock();
    locked = true;
}

void mutex::scoped_lock::unlock()
{
    __mutex.unlock();
    locked = false;
}

mutex::scoped_lock::~scoped_lock()
{
    if(locked && pthread_mutex_unlock(&__mutex.__mutex))
    {
        assert(false && "mutex::scoped_lock::~scoped_lock()");
        mlog(mlog::critical) << "mutex::scoped_lock::~scoped_lock() unlock mutex error";
    }
}

condition::condition()
{
    if(pthread_cond_init(&__condition, nullptr))
        throw str_exception("create condition error");
}

condition::~condition()
{
    if(pthread_cond_destroy(&__condition))
    {
        assert(false && "condition::~condition()");
        mlog(mlog::critical) << "condition::~condition() destroy condition error";
    }
}

void condition::wait(mutex::scoped_lock& lock)
{
    if(pthread_cond_wait(&__condition, &lock.__mutex.__mutex))
        throw str_exception("condition::wait() error");
}

void condition::timed_wait(mutex::scoped_lock& lock, uint32_t sec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += sec;
    int r = pthread_cond_timedwait(&__condition, &lock.__mutex.__mutex, &t);
    if(r && r != ETIMEDOUT)
        throw str_exception("condition::timed_wait() error");
}

void condition::timed_uwait(mutex::scoped_lock& lock, uint32_t usec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += usec * 1000;
    if(t.tv_nsec >= int32_t(ttime_t::frac))
    {
        t.tv_sec += t.tv_nsec / ttime_t::frac;
        t.tv_nsec %= ttime_t::frac;
    }
    int r = pthread_cond_timedwait(&__condition, &lock.__mutex.__mutex, &t);
    if(r && r != ETIMEDOUT)
        throw str_exception("condition::timed_uwait() error");
}

void condition::notify_one()
{
    if(pthread_cond_signal(&__condition))
        throw str_exception("condition::notify_one() error");
}

void condition::notify_all()
{
    if(pthread_cond_broadcast(&__condition))
        throw str_exception("condition::notify_all() error");
}

thread_local uint32_t cur_tid = 0;

struct free_threads
{
    queue<uint32_t> ids;
    mutex __mutex;

    free_threads()
    {
        for(uint32_t i = 1; i != max_threads_count; ++i)
            ids.push_back(i);
    }
    void set()
    {
        if(cur_tid)
            return;

        scoped_lock lock(__mutex);
        if(ids.empty())
            throw str_exception("set_thread_id() max_threads_count limit exceed");

        cur_tid = ids.front();
        ids.pop_front();
    }
    void free()
    {
        scoped_lock lock(__mutex);
        assert(cur_tid);
        ids.push_back(cur_tid);
    }
};

free_threads* free_threads_ptr = nullptr;

void* init_free_threads()
{
    assert(!free_threads_ptr);
    free_threads_ptr = new free_threads;
    return free_threads_ptr;
}

void delete_free_threads(void* ptr)
{
    delete (free_threads*)ptr;
}

void set_free_threads(void* ptr)
{
    free_threads_ptr = (free_threads*)ptr;
}

void set_thread_id()
{
    free_threads_ptr->set();
}

uint32_t get_thread_id()
{
    return cur_tid;
}

void set_affinity_thread(uint32_t thrd)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thrd, &cpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
        mlog(mlog::critical) << "pthread_setaffinity_np() error";
}

static const uint32_t trash_t1 = 3, trash_t2 = 9;

void set_trash_thread()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(trash_t1, &cpuset);
    CPU_SET(trash_t2, &cpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
        cerr_write("pthread_setaffinity_np() error\n");
}

void set_significant_thread()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for(uint32_t i = 0; i != 11; ++i)
    {
        if(i != trash_t1 && i != trash_t2)
            CPU_SET(i, &cpuset);
    }
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
        mlog(mlog::critical) << "pthread_setaffinity_np() error";
}

void* thread_f(void *p)
{
    set_thread_id();
    unique_ptr<thread_func> f(reinterpret_cast<thread_func*>(p));
    f->run();
    free_threads_ptr->free();
    return nullptr;
}

uint64_t thread_create(thread_func* p)
{
    pthread_t tid = 0;
    int ret = pthread_create(&tid, nullptr, &thread_f, p);
    if(ret)
        throw str_exception("pthread_create error");
    return tid;
}

thread::thread() : tid()
{
}

thread::~thread()
{
    join();
}

thread::thread(thread&& r)
{
    tid = r.tid;
    r.tid = 0;
}

thread& thread::operator=(thread&& r)
{
    if(tid)
        throw str_exception("thread already initialized");
    tid = r.tid;
    r.tid = 0;
    return *this;
}

void thread::swap(thread& r)
{
    simple_swap(tid, r.tid);
}

bool thread::joinable() const
{
    return !!tid;
}

void thread::join()
{
    if(!tid)
        return;
    if(pthread_join(tid, 0))
    {
        tid = 0;
        throw str_exception("thread::join error");
    }
    tid = 0;
}

void thread::detach()
{
    pthread_detach(tid);
    tid = 0;
}

volatile bool can_run = true;
signals_holder::signal_f on_term_signal = nullptr;

void term_signal(int sign)
{
    mlog() << "Termination signal received: " << sign;
    can_run = false;
    if(on_term_signal)
        on_term_signal(sign);
}

void on_signal(int sign)
{
    mlog() << "Signal received: " << sign;
}

void on_usr_signal(int sign)
{
    mlog(mlog::critical) << "Signal usr received: " << sign;
    profilerinfo::instance().print(mlog::critical);
}

void enable_core_dump()
{
    rlimit corelim = rlimit();
    corelim.rlim_cur = RLIM_INFINITY;
    corelim.rlim_max = RLIM_INFINITY;
    if(setrlimit(RLIMIT_CORE, &corelim))
        throw str_exception("enable_core_dump() error");
}

signals_holder::signals_holder(signal_f term_signal_func, signal_f usr_signal_func)
{
    enable_core_dump();
    on_term_signal = term_signal_func;
    std::signal(SIGTERM, &term_signal);
    std::signal(SIGINT, &term_signal);
    std::signal(SIGHUP, &on_signal);
    std::signal(SIGPIPE, &on_signal);
    std::signal(SIGUSR1, usr_signal_func ? usr_signal_func : &on_usr_signal);
}

signals_holder::~signals_holder()
{
    std::signal(SIGTERM, SIG_DFL);
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGHUP, SIG_DFL);
    std::signal(SIGPIPE, SIG_DFL);
    std::signal(SIGUSR1, SIG_DFL);
}

