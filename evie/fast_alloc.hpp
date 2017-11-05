#pragma once

#include "log.hpp"

#include <cstdint>
#include <vector>
#include <mutex>

template<typename ttype, uint32_t pool_size = 16 * 1024>
struct fast_alloc
{
    typedef ttype type;

private:
    uint32_t allocated_size;
    //TODO: move bufs to lockfree something
    std::vector<type*> bufs;
    std::mutex mutex;

    type* my_alloc()
    {
        if(allocated_size > pool_size)
            throw std::runtime_error(es() % "fast_alloc memory limit exceeted, allocated size: " % allocated_size);

        ++allocated_size;
        type* m = new type;
        return m;
    }
public:

    fast_alloc() : allocated_size(), bufs(pool_size)
    {
        for(type*& m: bufs)
            m = my_alloc();
    }
    type* alloc()
    {
        std::unique_lock<std::mutex> lock(mutex);
        if(!bufs.empty()) {
            type* m = bufs.back();
            bufs.pop_back();
            return m;
        }
        return my_alloc();
    }
    void free(type* m)
    {
        std::unique_lock<std::mutex> lock(mutex);
        bufs.push_back(m);
    }
    ~fast_alloc()
    {
        for(type*& m: bufs)
            delete m;
    }
};

template<typename alloc>
class alloc_holder
{
    alloc &a;
    typedef typename alloc::type type;
    type* p;
public:
    alloc_holder(alloc& a) : a(a), p(a.alloc())
    {
    }
    ~alloc_holder()
    {
        if(p)
            a.free(p);
    }
    type* operator->()
    {
        return p;
    }
    type& operator*()
    {
        return *p;
    }
    type* release()
    {
        type* ret = p;
        p = nullptr;
        return ret;
    }
};

