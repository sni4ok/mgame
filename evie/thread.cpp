/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "thread.hpp"
#include "mlog.hpp"

#include <pthread.h>

my_mutex::my_mutex()
{
    if(pthread_mutex_init(&mutex, NULL))
        throw str_exception("create mutex error");
}

my_mutex::~my_mutex()
{
    pthread_mutex_destroy(&mutex);
}

void my_mutex::lock()
{
    if(pthread_mutex_lock(&mutex))
        throw str_exception("lock mutex error");
}

bool my_mutex::try_lock()
{
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
}

void my_mutex::scoped_lock::lock()
{
    mutex.lock();
}

void my_mutex::scoped_lock::unlock()
{
    mutex.unlock();
}

my_mutex::scoped_lock::~scoped_lock()
{
    unlock();
}

my_condition::my_condition()
{
    if(pthread_cond_init(&condition, NULL))
        throw str_exception("create condition error");
}

my_condition::~my_condition()
{
    pthread_cond_destroy(&condition);
}

void my_condition::wait(my_mutex::scoped_lock& lock)
{
    pthread_cond_wait(&condition, &lock.mutex.mutex);
}

void my_condition::timed_wait(my_mutex::scoped_lock& lock, uint32_t sec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += sec;
    pthread_cond_timedwait(&condition, &lock.mutex.mutex, &t);
}

void my_condition::timed_uwait(my_mutex::scoped_lock& lock, uint32_t usec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    uint32_t sec = usec / 1000000;
    t.tv_sec += sec;
    long nsec = (usec - sec * 1000000) * 1000;
    t.tv_nsec += nsec;
    pthread_cond_timedwait(&condition, &lock.mutex.mutex, &t);
}

void my_condition::notify_one()
{
    pthread_cond_signal(&condition);
}

void my_condition::notify_all()
{
    pthread_cond_broadcast(&condition);
}

uint32_t thread_tss_id = 0;

inline uint32_t atomic_inc_tss()
{
    return __atomic_add_fetch(&thread_tss_id, uint32_t(1), __ATOMIC_RELAXED);
}

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void make_key()
{
    pthread_key_create(&key, NULL);
}

int get_thread_id()
{
    void *ptr;
    pthread_once(&key_once, make_key);
    if((ptr = pthread_getspecific(key)) == NULL)
    {
        ptr = new int(atomic_inc_tss());
        pthread_setspecific(key, ptr);
    }
    return *((int*)ptr);
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
        mlog(mlog::critical) << "pthread_setaffinity_np() error";
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

