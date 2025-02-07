/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mmap.hpp"

#include "../evie/utils.hpp"
#include "../evie/mfile.hpp"
#include "../evie/mlog.hpp"

#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>

inline void init_smc(void* ptr)
{
    shared_memory_sync* p = get_smc(ptr);
    int ret = 0;
	pthread_condattr_t cond_attr;
    ret &= pthread_condattr_init(&cond_attr);
    ret &= pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    ret &= pthread_cond_init(&(p->condition), &cond_attr);
    ret &= pthread_condattr_destroy(&cond_attr);

    pthread_mutexattr_t m_attr;
    ret &= pthread_mutexattr_init(&m_attr);
    ret &= pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
    ret &= pthread_mutex_init(&(p->mutex), &m_attr);
    ret &= pthread_mutexattr_destroy(&m_attr);
    if(ret)
    {
        mmap_close(ptr);
        throw mexception("init_smc error");
    }
}

uint8_t mmap_nusers(char_cit params)
{
    int h = ::open(params, O_RDONLY, 0666);
    if(h <= 0)
        throw_system_failure(es() % "mmap open " % _str_holder(params) % " error");
    uint8_t c;
    bool r = lseek(h, mmap_alloc_size, SEEK_SET) >= 0;
    r &= (read(h, &c, 1) == 1);
    ::close(h);
    if(!r)
        throw_system_failure(es() % "mmap read " % _str_holder(params) % " error");
    return c - '1' + 1;
}

void* mmap_create(char_cit params, bool create)
{
    if(create)
    {
        if(is_file_exist(params))
        {
            mlog(mlog::critical) << "remove_file: " << _str_holder(params);
            {
                int h = ::open(params, O_WRONLY, 0666);
                ssize_t res = write(h, "", 1);
                my_unused(res);
                ::close(h);
            }
            remove_file(params);
        }
    }
    int h = ::open(params, create ? O_RDWR | O_CREAT | O_TRUNC: O_RDWR, 0666);
    if(h <= 0)
        throw_system_failure(es() % "open " % _str_holder(params) % " error");
    if(create)
    {
        bool r = lseek(h, mmap_alloc_size, SEEK_SET) >= 0;
        r &= (write(h, "1", 1) == 1);
        r &= (lseek(h, 0, SEEK_SET) >= 0);
        if(!r) {
            ::close(h);
            throw_system_failure(es() % "mmap creating file error " % _str_holder(params));
        }
    }
    else
    {
        struct stat st;
        int res = ::fstat(h, &st);
        if(res || (st.st_size != mmap_alloc_size + 1))
        {
            ::close(h);
            if(res)
                throw_system_failure(es() % "fstat() error for " % _str_holder(params));
            else
                throw mexception(es() % "mmap_create error for " % _str_holder(params) % ", file_sz: " % int64_t(st.st_size));
        }
    }

    void* p = mmap(NULL, mmap_alloc_size, PROT_READ | PROT_WRITE, MAP_SHARED, h, 0);
    ::close(h);
    if(p == (void*)-1)
        throw_system_failure(es() % "mmap error for " % _str_holder(params));

    if(create)
        init_smc(p);

    return p;
}

void mmap_close(void* ptr)
{
    munmap(ptr, mmap_alloc_size);
}

pthread_lock::pthread_lock(pthread_mutex_t& mutex) : mutex(&mutex), mlock()
{
    lock();
}

pthread_lock::~pthread_lock()
{
    if(mlock)
        pthread_mutex_unlock(mutex);
}

void pthread_lock::lock()
{
    assert(!mlock);
    if(pthread_mutex_lock(mutex))
        throw str_exception("mmap::mmap_lock error");
    mlock = true;
}

void pthread_lock::unlock()
{
    assert(mlock);
    pthread_mutex_unlock(mutex);
    mlock = false;
}

void pthread_timedwait(pthread_cond_t& condition, pthread_mutex_t& mutex, uint32_t sec)
{
    timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += sec;
    pthread_cond_timedwait(&condition, &mutex, &t);
}

