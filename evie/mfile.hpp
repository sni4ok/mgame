/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "time.hpp"
#include "vector.hpp"

struct mfile
{
    int hfile;

    mfile(int hfile);
    mfile(const char* file);
    mfile(const mfile&) = delete;
    uint64_t size() const;
    void seekg(uint64_t pos);
    void read(char* ptr, uint64_t size);
    ~mfile();

    void swap(mfile& r);
};

bool read_file(mvector<char>& buf, const char* fname, bool can_empty = false);
mvector<char> read_file(const char* fname, bool can_empty = false);
void write_file(const char* fname, const char* buf, uint64_t size, bool trunc);
bool is_file_exist(const char* fname, uint64_t* fsize = nullptr);
uint64_t file_size(const char* fname);
void remove_file(const char* fname);
void rename_file(const char* from, const char* to);
bool create_directory(const char* fname);
void create_directories(const char* fname);
bool is_directory(const char* fname);
ttime_t get_file_mtime(const char* fname);

