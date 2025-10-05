/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include <sys/mman.h>
#include <pthread.h>

struct shared_memory_sync
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    u8 pooling_mode; //0 unitialized, 1 pooling mode on, 2 pooling mode off
};

struct pthread_lock
{
    pthread_mutex_t* mutex;
    bool mlock;

    pthread_lock(pthread_mutex_t& mutex);
    pthread_lock(const pthread_lock&) = delete;
    ~pthread_lock();
    void lock();
    void unlock();
};

void pthread_timedwait(pthread_cond_t& condition, pthread_mutex_t& mutex, u32 sec);

static const u32 mmap_alloc_size_base = message_size + (message_size - 2) * 255 * message_size;
static const u32 mmap_alloc_size = mmap_alloc_size_base + sizeof(shared_memory_sync);

inline shared_memory_sync* get_smc(void* ptr)
{
    return (shared_memory_sync*)(((u8*)ptr) + mmap_alloc_size_base);
}

void* mmap_create(const char* params, bool create);
void mmap_close(void* ptr);
u8 mmap_nusers(const char* params);

inline void mmap_store(u8* ptr, u8 value)
{
    __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

inline u8 mmap_load(u8* ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

inline bool mmap_compare_exchange(u8* ptr, u8 from, u8 to)
{
    return __atomic_compare_exchange_n(ptr, &from, to, false/*weak*/, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

