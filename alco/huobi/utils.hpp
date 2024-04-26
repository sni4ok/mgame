/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <zlib.h>

template<typename alloc>
voidpf zlib_alloc_func(voidpf ze, uInt items, uInt size)
{
    return ((alloc*)(ze))->alloc(items, size);
}

void zlib_free_func(voidpf, voidpf)
{
}

struct zlib_alloc
{
    mvector<char> buf;
    uint32_t allocated;

    zlib_alloc() : buf(4 * 1024 * 1024), allocated()
    {
    }
    void* alloc(uInt items, uInt size)
    {
        uint32_t req = items * size;
        if(allocated + req > buf.size()) [[unlikely]]
            throw str_exception("zlibe::alloc() bad alloc");
        char* ret = &buf[allocated];
        allocated += req;
        return (void*)ret;
    }
    void decompress_pre()
    {
        allocated = 0;
    }
};

struct zlib_alloc_base
{
};

template<bool z_alloc>
struct zlib_impl : std::conditional_t<z_alloc, zlib_alloc, zlib_alloc_base>
{
    mvector<char> dest;
    z_stream strm;

    zlib_impl() : dest(1024 * 1024), strm()
    {
        if constexpr(z_alloc)
        {
            strm.zalloc = zlib_alloc_func<zlib_alloc>;
            strm.zfree = zlib_free_func;
        }
        strm.opaque = this;
    }
    str_holder decompress(const char* p, uint32_t size)
    {
        if constexpr(z_alloc)
            this->decompress_pre();

        strm.next_in = (Bytef*)p;
        strm.avail_in = size;

        strm.next_out = (Bytef*)(&dest[0]);
        strm.avail_out = dest.size();

        int err = inflateInit2(&strm, MAX_WBITS + 16);
        if(err != Z_OK) [[unlikely]]
            throw str_exception("inflateInit2 error");
        
        err = inflate(&strm, Z_FINISH);
        if(err == Z_STREAM_END) [[likely]] {
            return str_holder(&dest[0], strm.total_out);
        }
        throw mexception(es() % "zlib::inflate error for size: " % size);
    }
};

typedef zlib_impl<true> zlibe;

