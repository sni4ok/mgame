/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages.hpp"

#include <stdexcept>

#include <sys/mman.h>
#include <pthread.h>

struct shared_memory_sync
{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    uint8_t pooling_mode; //0 unitialized, 1 pooling mode
};

static const uint32_t mmap_alloc_size_base = message_size + (message_size - 2) * 255 * message_size;
static const uint32_t mmap_alloc_size = mmap_alloc_size_base + sizeof(shared_memory_sync);

inline shared_memory_sync* get_smc(void* ptr)
{
    return  (shared_memory_sync*)(((uint8_t*)ptr) + mmap_alloc_size_base);
}

void* mmap_create(const char* params, bool create);

inline void mmap_close(void* ptr)
{
    munmap(ptr, mmap_alloc_size);
}

