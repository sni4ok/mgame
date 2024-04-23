/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <tuple>
#include <cstdint>

#include "bits/pthreadtypes.h"

struct my_mutex
{
    pthread_mutex_t mutex;

    my_mutex();
    ~my_mutex();
    my_mutex(const my_mutex&) = delete;
    void lock();
    void unlock();
    bool try_lock();

    struct scoped_lock
    {
        my_mutex& mutex;
        bool locked;

        scoped_lock(my_mutex& mutex);
        ~scoped_lock();
        void lock();
        void unlock();
    };
};

struct my_condition
{
    pthread_cond_t condition;

    my_condition();
    ~my_condition();
    my_condition(const my_condition&) = delete;

    void wait(my_mutex::scoped_lock& lock);
    void timed_wait(my_mutex::scoped_lock& lock, uint32_t sec);
    void timed_uwait(my_mutex::scoped_lock& lock, uint32_t usec);
    void notify_one();
    void notify_all();
};

static const uint32_t max_threads_count = 100;

void* init_free_threads();
void delete_free_threads(void* ptr);
void set_free_threads(void* ptr);
void set_thread_id();
uint32_t get_thread_id();
void set_affinity_thread(uint32_t thrd);
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
        std::apply(f, p);
    }
};

uint64_t thread_create(thread_func* p);

struct thread
{
    uint64_t tid;

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
        auto v = std::make_tuple(args...);
        thread_func* p = new t_func<func, decltype(v)>(f, v);
        tid = thread_create(p);
    }
};

typedef thread jthread;

template<typename type>
struct ref
{
    type* t;

    ref(type& t) : t(&t)
    {
    }
    operator type&()
    {
        return *t;
    }
};

