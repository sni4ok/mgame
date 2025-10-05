/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "tuple.hpp"

#include <cstdint>

#include <bits/pthreadtypes.h>

struct mutex
{
    pthread_mutex_t __mutex;

    mutex();
    ~mutex();
    mutex(const mutex&) = delete;
    void lock();
    void unlock();
    bool try_lock();

    struct scoped_lock
    {
        mutex& __mutex;
        bool locked;

        scoped_lock(mutex& mutex);
        ~scoped_lock();
        void lock();
        void unlock();
    };
};

template<typename mutex>
struct scoped_lock : mutex::scoped_lock
{
    [[nodiscard]] scoped_lock(mutex& m) : mutex::scoped_lock(m)
    {
    }
};

struct critical_section
{
    bool flag;
    ::mutex mutex;
    static const u32 free_lock_count = 10;

    critical_section() : flag()
    {
    }
    bool try_lock()
    {
        return !__atomic_test_and_set(&flag, __ATOMIC_ACQUIRE);
    }
    [[nodiscard]] bool lock()
    {
        if(try_lock()) [[likely]]
            return true;
        for(u32 i = 0; i != free_lock_count; ++i)
            if(try_lock())
                return true;

        { [[unlikely]]
            mutex.lock();
            for(;;)
                if(try_lock())
                    return false;
        }
    }
    void unlock(bool free_lock)
    {
        __atomic_clear(&flag, __ATOMIC_RELEASE);

        if(!free_lock) [[unlikely]]
            mutex.unlock();
    }

    struct scoped_lock
    {
        critical_section& cs;
        bool free_lock;

        scoped_lock(critical_section& cs) : cs(cs)
        {
            free_lock = cs.lock();
        }
        ~scoped_lock()
        {
            cs.unlock(free_lock);
        }
    };

    template<bool use_mt>
    struct mt_lock;
};

template<>
struct critical_section::mt_lock<true> : critical_section::scoped_lock
{
    using scoped_lock::scoped_lock;
};

template<>
struct critical_section::mt_lock<false>
{
    mt_lock(critical_section&)
    {
    }
};

struct condition
{
    pthread_cond_t __condition;

    condition();
    ~condition();
    condition(const condition&) = delete;

    void wait(mutex::scoped_lock& lock);
    void timed_wait(mutex::scoped_lock& lock, u32 sec);
    void timed_uwait(mutex::scoped_lock& lock, u32 usec);
    void notify_one();
    void notify_all();
};

static const u32 max_threads_count = 100;

void* init_free_threads();
void delete_free_threads(void* ptr);
void set_free_threads(void* ptr);
void set_thread_id();
u32 get_thread_id();
void set_affinity_thread(u32 thrd);
void set_trash_thread();
void set_significant_thread();

struct thread_func
{
    virtual void run() = 0;

    virtual ~thread_func()
    {
    }
};

template<typename func, typename params>
struct t_func : thread_func
{
    func f;
    params p;

    t_func(func f, const params& p) : f(f), p(p)
    {
    }

    void run()
    {
        apply(f, p);
    }
};

u64 thread_create(thread_func* p);

struct thread
{
    u64 tid;

    thread();
    thread(thread&& r);
    thread& operator=(thread&& r);
    thread(const thread&) = delete;
    void swap(thread& r);
    bool joinable() const;
    void join();
    void detach();
    ~thread();

    template<typename func, typename ... params>
    thread(func f, params ... args)
    {
        auto v = make_tuple(args...);
        thread_func* p = new t_func<func, decltype(v)>(f, v);
        tid = thread_create(p);
    }
};

typedef thread jthread;

