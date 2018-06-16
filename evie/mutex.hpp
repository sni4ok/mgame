/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <condition_variable>
#include <shared_mutex>

struct my_mutex : std::mutex
{
    typedef std::unique_lock<std::mutex> scoped_lock;
};

template<typename c>
struct my_condition_impl
{
    c cond;

    void wait(my_mutex::scoped_lock& lock){
        cond.wait(lock);
    }
    template<typename lock>
    void timed_wait(lock& l, uint32_t sec){
        cond.wait_for(l, std::chrono::seconds(sec));
    }
    template<typename lock>
    void timed_uwait(lock& l, uint32_t usec){
        cond.wait_for(l, std::chrono::microseconds(usec));
    }
    void notify_one(){
        cond.notify_one();
    }
    void notify_all(){
        cond.notify_all();
    }
};

typedef my_condition_impl<std::condition_variable> my_condition;
typedef my_condition_impl<std::condition_variable_any> my_condition_any;

class my_mutex_lock
{
    my_mutex& mutex;
    bool lock;
public:
    my_mutex_lock(my_mutex& mutex, bool lock = true) : mutex(mutex), lock(lock){
        if(lock)
            mutex.lock();
    }
    void unlock(){
        if(lock){
            mutex.unlock();
            lock = false;
        }
    }
    ~my_mutex_lock(){
        if(lock)
            mutex.unlock();
    }
};

class my_event
{
    my_condition condition;
    my_mutex mutex;
    bool notify_on;

public:
    my_event() : notify_on(false) {
    }
    void notify() {
        my_mutex::scoped_lock lock(mutex);
        notify_on = true;
        condition.notify_all();
    }
    void wait() {
        my_mutex::scoped_lock lock(mutex);
        if(!notify_on)
            condition.wait(lock);
        notify_on = false;
    }
    void timed_wait(uint32_t sec) {
        my_mutex::scoped_lock lock(mutex);
        if(!notify_on)
            condition.timed_wait(lock, sec);
        notify_on = false;
    }
    void timed_uwait(uint32_t usec) {
        my_mutex::scoped_lock lock(mutex);
        if(!notify_on)
            condition.timed_wait(lock, usec);
        notify_on = false;
    }
};

