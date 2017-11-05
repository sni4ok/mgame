#pragma once

#include "string.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cassert>

template <typename type>
type lexical_cast(const std::string& str)
{
    type var;
    std::istringstream iss;
    iss.str(str);
    iss >> var;
    return var;
}

inline void throw_system_failure(const char* msg)
{
    throw std::runtime_error(msg);
}

struct noncopyable
{
    noncopyable() {
    }
private:
    noncopyable(const noncopyable&) = delete;
};

template<typename base>
class stack_singleton : noncopyable
{
    static base*& get_impl() {
        static base* value = 0;
        return value;
    }
public:
    static void set_instance(base* instance) {
        assert((!get_impl() || get_impl() == instance) && "singleton already initialized");
        get_impl() = instance;
    }
    static base& instance() {
        return *get_impl();
    }
    stack_singleton() {
        set_instance(static_cast<base*>(this));
    }
    ~stack_singleton() {
        get_impl() = 0;
    }
};

class es
{
    //this class used for formating exceptions
    std::string s;
    std::stringstream ss;
public:
    template<typename t>
    es& operator%(const t& v) {
        ss << v;
        return *this;
    }
    operator const char*() {
        s = ss.str();
        return s.c_str();
    }
};


template<typename string>
void read_file(string& buf, const char* fname, bool can_empty = false)
{
    uint32_t buf_size = buf.size();
    std::ifstream f(fname);
    if(f) {
        f.exceptions(std::ios::badbit);
        std::copy(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>(), std::back_inserter(buf));
    }
    if(!can_empty && buf_size == buf.size())
        throw std::runtime_error(es() % "read \"" % fname % "\" error");
}

template<typename string>
string read_file(const char* fname, bool can_empty = false)
{
    string buf;
    read_file(buf, fname, can_empty);
    return std::move(buf);
}

static std::vector<std::string> split(const std::string& str, char sep = ',')
{
    std::vector<std::string> ret;
    if(!str.empty()) {
        auto it = str.begin(), ie = str.end(), i = it;
        while(it != ie) {
            i = std::find(it, ie, sep);
            ret.push_back(std::string(it, i));
            if(i != ie)
                ++i;
            it = i;
        }
    }
    return std::move(ret);
}

class crc32_table : noncopyable
{
    uint32_t crc_table[256];
    crc32_table() {
        for(uint32_t i = 0; i != 256; ++i) {
            uint32_t crc = i;
            for (uint32_t j = 0; j != 8; j++)
                crc = crc & 1 ? (crc >> 1) ^ 0xedb88320ul : crc >> 1;
            crc_table[i] = crc;
        }
    }
public:
    static uint32_t* get(){
        static crc32_table t;
        return t.crc_table;
    }
};

struct crc32
{
    uint32_t *crc_table, crc;
    crc32(uint32_t init) : crc_table(crc32_table::get()), crc(init ^ 0xFFFFFFFFUL) {
    }
    void process_bytes(const char* p, uint32_t len) {
        const unsigned char* buf = reinterpret_cast<const unsigned char*>(p);
        while(len--)
            crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    }
    uint32_t checksum() const {
        return crc ^ 0xFFFFFFFFUL;
    }
};

