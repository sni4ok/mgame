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
    uint8_t pooling_mode; //0 unitialized, 1 pooling mode on, 2 pooling mode off
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

void pthread_timedwait(pthread_cond_t& condition, pthread_mutex_t& mutex, uint32_t sec);

static const uint32_t mmap_alloc_size_base = message_size + (message_size - 2) * 255 * message_size;
static const uint32_t mmap_alloc_size = mmap_alloc_size_base + sizeof(shared_memory_sync);

inline shared_memory_sync* get_smc(void* ptr)
{
    return (shared_memory_sync*)(((uint8_t*)ptr) + mmap_alloc_size_base);
}

void* mmap_create(const char* params, bool create);
void mmap_close(void* ptr);
uint8_t mmap_nusers(const char* params);

inline void mmap_store(uint8_t* ptr, uint8_t value)
{
    __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

inline uint8_t mmap_load(uint8_t* ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

inline bool mmap_compare_exchange(uint8_t* ptr, uint8_t from, uint8_t to)
{
    return __atomic_compare_exchange_n(ptr, &from, to, false/*weak*/, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

