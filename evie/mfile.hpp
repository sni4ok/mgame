/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "utils.hpp"

#include <fstream>
#include <memory>

class mfile
{
    struct impl;
    std::unique_ptr<impl> pimpl;
public:
    mfile();
    mfile(const char* file, bool direct);
    bool open(const char* file, bool direct);
    uint64_t size() const;
    void seekg(uint64_t pos);
    void read(char* ptr, uint32_t size);
    ~mfile();
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
        throw std::runtime_error(es() % "read \"" % str_holder(fname) % "\" error");
}

template<typename string>
string read_file(const char* fname, bool can_empty = false)
{
    string buf;
    read_file(buf, fname, can_empty);
    return std::move(buf);
}

