/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mmap.hpp"

#include "evie/utils.hpp"

#include <unistd.h>
#include <fcntl.h>

void set_export_mtime(message* m)
{
    (m - 1)->t.time = get_cur_ttime();
}

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
    if(ret) {
        mmap_close(ptr);
        throw std::runtime_error("init_smc error");
    }
}

void* mmap_create(const char* params, bool create)
{
    int h = ::open(params, create ? (O_RDWR | O_CREAT | O_TRUNC) : O_RDWR, 0666);
    if(h <= 0)
        throw_system_failure(es() % "open " % _str_holder(params) % " error");

    bool r = lseek(h, mmap_alloc_size, SEEK_SET) >= 0;
    r &= (write(h, "", 1) == 1);
    r &= (lseek(h, 0, SEEK_SET) >= 0);
    if(!r)
        throw_system_failure(es() % "mmap creating file error " % _str_holder(params));

    void* p = mmap(NULL, mmap_alloc_size, PROT_READ | PROT_WRITE, MAP_SHARED, h, 0);
    ::close(h);
    if(!p)
        throw_system_failure(es() % "mmap error for " % _str_holder(params));

    if(create)
        init_smc(p);

    return p;
}

