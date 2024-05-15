/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "thread.hpp"
#include "atomic.hpp"
#include "mlog.hpp"
#include "smart_ptr.hpp"
#include "profiler.hpp"
#include "queue.hpp"

#include <cassert>

#include <pthread.h>
#include <errno.h>

my_mutex::my_mutex()
{
    if(pthread_mutex_init(&mutex, NULL))
        throw str_exception("create mutex error");
}

my_mutex::~my_mutex()
{
    if(pthread_mutex_destroy(&mutex))
    {
        assert(false && "my_mutex::~my_mutex()");
        mlog(mlog::critical) << "my_mutex::~my_mutex() pthread_mutex_destroy()";
    }
}

void my_mutex::lock()
{
    //MPROFILE("mutex::lock")
    if(pthread_mutex_lock(&mutex))
        throw str_exception("lock mutex error");
}

bool my_mutex::try_lock()
{
    //MPROFILE("mutex::try_lock")
    if(pthread_mutex_trylock(&mutex))
        return false;
    return true;
}

void my_mutex::unlock()
{
    if(pthread_mutex_unlock(&mutex))
        throw str_exception("unlock mutex error");
}

my_mutex::scoped_lock::scoped_lock(my_mutex& mutex) : mutex(mutex)
{
    lock();
    locked = true;
}

void my_mutex::scoped_lock::lock()
{
    mutex.lock();
    locked = true;
}

void my_mutex::scoped_lock::unlock()
{
    mutex.unlock();
    locked = false;
}

my_mutex::scoped_lock::~scoped_lock()
{
    if(locked && pthread_mutex_unlock(&mutex.mutex))
    {
        assert(false && "my_mutex::scoped_lock::~scoped_lock()");
        mlog(mlog::critical) << "my_mutex::scoped_lock::~scoped_lock() unlock mutex error";
    }
}

my_condition::my_condition()
{
    if(pthread_cond_init(&condition, NULL))
        throw str_exception("create condition error");
}

my_condition::~my_condition()
{
    if(pthread_cond_destroy(&condition))
    {
        assert(false && "my_condition::~my_condition()");
        mlog(mlog::critical) << "my_condition::~my_condition() destroy condition error";
    }
}

void my_condition::wait(my_mutex::scoped_lock& lock)
{
    if(pthread_cond_wait(&condition, &lock.mutex.mutex))
        throw str_exception("my_condition::wait() error");
}

void my_condition::timed_wait(my_mutex::scoped_lock& lock, uint32_t sec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += sec;
    int r = pthread_cond_timedwait(&condition, &lock.mutex.mutex, &t);
    if(r && r != ETIMEDOUT)
        throw str_exception("my_condition::timed_wait() error");
}

void my_condition::timed_uwait(my_mutex::scoped_lock& lock, uint32_t usec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_nsec += usec * 1000;
    if(t.tv_nsec >= int32_t(ttime_t::frac))
    {
        t.tv_sec += t.tv_nsec / ttime_t::frac;
        t.tv_nsec %= ttime_t::frac;
    }
    int r = pthread_cond_timedwait(&condition, &lock.mutex.mutex, &t);
    if(r && r != ETIMEDOUT)
        throw str_exception("my_condition::timed_uwait() error");
}

void my_condition::notify_one()
{
    if(pthread_cond_signal(&condition))
        throw str_exception("my_condition::notify_one() error");
}

void my_condition::notify_all()
{
    if(pthread_cond_broadcast(&condition))
        throw str_exception("my_condition::notify_all() error");
}

thread_local uint32_t cur_tid = 0;

struct free_threads
{
    queue<uint32_t> ids;
    my_mutex mutex;

    free_threads()
    {
        for(uint32_t i = 1; i != max_threads_count; ++i)
            ids.push_back(i);
    }
    void set() {
        if(cur_tid)
            return;
        my_mutex::scoped_lock lock(mutex);
        if(ids.empty())
            throw str_exception("set_thread_id() max_threads_count limit exceed");
        cur_tid = ids.front();
        ids.pop_front();
    }
    void free() {
        my_mutex::scoped_lock lock(mutex);
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

