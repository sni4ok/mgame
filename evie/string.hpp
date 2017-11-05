#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>

struct buf_stream
{
    char *from, *cur, *to;
    buf_stream(std::vector<char>& v) : from(&v[0]), cur(from), to(from + v.size()){
    }
    buf_stream(char* from, char* to) : from(from), cur(from), to(to){
    }
    const char* begin() const{
        return from;
    }
    uint32_t size() const{
        return cur - from;
    }
    void clear(){
        cur = from;
    }
    void write(const char* v, uint32_t s){
        if(s > to - cur)
            throw std::runtime_error("buf_stream::write() overloaded");
        memcpy(cur, v, s);
        cur += s;
    }
    void put(char c){
        if(1 > to - cur)
            throw std::runtime_error("buf_stream::put() overloaded");
        *cur = c;
        ++cur;
    }
};

