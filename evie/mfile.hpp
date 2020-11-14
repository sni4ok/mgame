/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "utils.hpp"

struct mfile : noncopyable
{
    int hfile;
    mfile(int hfile);
    mfile(const char* file);
    uint64_t size() const;
    void seekg(uint64_t pos);
    void read(char* ptr, uint32_t size);
    ~mfile();
};

bool read_file(std::vector<char>& buf, const char* fname, bool can_empty = false);
std::vector<char> read_file(const char* fname, bool can_empty = false);

