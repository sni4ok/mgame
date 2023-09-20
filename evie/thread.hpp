/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

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

void set_significant_thread();
void set_trash_thread();
int get_thread_id();

