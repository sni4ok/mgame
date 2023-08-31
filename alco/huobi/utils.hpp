
#include <zlib.h>

voidpf zlib_alloc_func(voidpf opaque, uInt items, uInt size);
void zlib_free_func(voidpf, voidpf)
{
}

struct zlibe
{
    std::vector<char> buf, dest;
    uint32_t allocated;
    z_stream strm;

    zlibe() : buf(4 * 1024 * 1024), dest(1024 * 1024), allocated()
    {
        strm.zalloc = zlib_alloc_func;
        strm.zfree = zlib_free_func;
        strm.opaque = this;
    }
    void* alloc(uInt items, uInt size)
    {
        uint32_t req = items * size;
        if(unlikely(allocated + req > buf.size()))
            throw std::runtime_error("zlibe::alloc() bad alloc");
        char* ret = &buf[allocated];
        allocated += req;
        return (void*)ret;
    }
    str_holder decompress(const char* p, uint32_t size)
    {
        allocated = 0;
        
        strm.next_in = (Bytef*)p;
        strm.avail_in = size;

        strm.next_out = (Bytef*)(&dest[0]);
        strm.avail_out = dest.size();

        int err = inflateInit2(&strm, MAX_WBITS + 16);
        if(unlikely(err != Z_OK))
            throw std::runtime_error("inflateInit2 error");
        
        err = inflate(&strm, Z_FINISH);
        if(likely(err == Z_STREAM_END)){
            return str_holder(&dest[0], strm.total_out);
        }
        throw std::runtime_error(es() % "zlib::inflate error for size: " % size);
    }
};

voidpf zlib_alloc_func(voidpf ze, uInt items, uInt size)
{
    return ((zlibe*)(ze))->alloc(items, size);
}

