#pragma once

#include "mutex.hpp"
#include "utils.hpp"

#include <pthread.h>

#include <list>

template<typename worker>
struct workers_simple
{
    void proceed(my_mutex::scoped_lock& lock, std::list<typename worker::Data>& datas)
    {
        typename worker::Data data = *datas.begin();

        datas.pop_front();
        lock.unlock();
        worker::Proceed(data);
        lock.lock();
    }
};

template<typename worker>
struct workers_circle
{
    void proceed(my_mutex::scoped_lock& lock, std::list<typename worker::Data>& datas)
    {
        typename worker::Data data = *datas.begin();

        datas.pop_front();
        lock.unlock();
        bool cont = worker::Proceed(data);
        lock.lock();
        if(cont)
            datas.push_back(std::move(data));
    }
};

template<typename Worker, typename worker = workers_simple<Worker> >
class Workers : public stack_singleton<Workers<Worker, worker> >
{
    volatile bool* can_run;
    volatile bool my_can_run;
    bool can_not_run_on_exit;
    bool can_exit;
    std::vector<std::thread> threads;
    std::list<typename Worker::Data> datas;
    my_mutex mutex;
    my_condition condition;

    void WorkThread(size_t worker_id, bool set_affinity)
    {
        if(set_affinity)
        {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(worker_id, &cpuset);
            if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
                mlog(mlog::critical) << "pthread_setaffinity_np() error";
        }

        my_mutex::scoped_lock lock(mutex);
        while(*can_run)
        {
            try
            {
                if(!datas.empty())
                {
                    worker().proceed(lock, datas);
                    continue;
                }
                else
                {
                    if(can_exit)
                        return;
                    condition.wait(lock);
                }
            }
            catch(std::exception& e)
            {
                mlog() << "WorkThread: " << e;
                lock.lock();
            }
        }
    }

public:
    Workers(volatile bool* can_run = NULL, bool can_not_run_on_exit = false)
        : can_run(can_run), my_can_run(true), can_not_run_on_exit(can_not_run_on_exit), can_exit()
    {
        if(!this->can_run)
            this->can_run = &my_can_run;
    }
    void Load(size_t threads_count, bool set_affinity = false)
    {
        for(size_t i = 0; i != threads_count; ++i)
            threads.push_back(std::thread(&Workers::WorkThread, this, i, set_affinity));
    }
    void Add(typename Worker::Data&& data)
    {
        my_mutex::scoped_lock lock(mutex);
        datas.push_back(std::move(data));
        condition.notify_one();
    }
    ~Workers()
    {
        if(can_not_run_on_exit)
            *can_run = false;
        
        my_mutex::scoped_lock lock(mutex);
        can_exit = true;
        condition.notify_all();
        lock.unlock();

        for(auto& t: threads)
            t.join();
    }
};

