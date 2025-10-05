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
    mfile(char_cit file);
    mfile(const mfile&) = delete;
    void swap(mfile& r);
    u64 size() const;
    void seekg(u64 pos);
    void read(char_it ptr, u64 size);
    ~mfile();
};

bool read_file(mvector<char>& buf, char_cit fname, bool can_empty = false);
bool read_file_and_truncate(mvector<char>& buf, char_cit fname, bool can_empty = false);
mvector<char> read_file(char_cit fname, bool can_empty = false);
void write_file(char_cit fname, char_cit buf, u64 size, bool trunc);
bool is_file_exist(char_cit fname, u64* fsize = nullptr);
u64 file_size(char_cit fname);
void remove_file(char_cit fname);
void rename_file(char_cit from, char_cit to);
bool create_directory(char_cit fname);
u32 create_directories(char_cit fname);
bool is_directory(char_cit fname);
ttime_t get_file_mtime(char_cit fname);

