/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mfile.hpp"
#include "string.hpp"
#include "mstring.hpp"
#include "algorithm.hpp"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/file.h>

mfile::mfile(const char* file)
{
    hfile = ::open(file, O_RDONLY);
    if(hfile <= 0)
        throw_system_failure(es() % "open file " % _str_holder(file) % " error");
}

mfile::mfile(int hfile) : hfile(hfile)
{
}

void mfile::swap(mfile& r)
{
    simple_swap(hfile, r.hfile);
}

uint64_t mfile::size() const
{
    struct stat st;
    if(::fstat(hfile, &st))
        throw_system_failure("fstat() error");
    return st.st_size;
}

void mfile::seekg(uint64_t pos)
{
    if(::lseek(hfile, pos, SEEK_SET) < 0)
        throw_system_failure("lseek() error");
}

void mfile::read(char* ptr, uint64_t size)
{
    uint64_t read_bytes = 0;
    while(read_bytes != size)
    {
        ssize_t r = ::read(hfile, ptr + read_bytes, size - read_bytes);
        if(r <= 0)
            throw_system_failure(es() % "read() error, read_bytes: " % read_bytes % ", required: " % size);
        read_bytes += r;
    }
}

mfile::~mfile()
{
    if(hfile)
        ::close(hfile);
}

inline bool read_file_impl(int hfile, bool trunc, mvector<char>& buf, const char* fname, bool can_empty)
{
    bool ret = false;
    uint64_t buf_size = buf.size();
    if(hfile > 0)
    {
        mfile f(hfile);
        uint64_t size = f.size();
        buf.resize(buf_size + size);
        f.read(buf.begin() + buf_size, size);
        ret = true;
        if(trunc && buf_size != buf.size())
        {
            if(ftruncate(hfile, 0))
                throw_system_failure(es() % "ftruncate file " % _str_holder(fname) % " error");
        }
    }
    if(!can_empty && buf_size == buf.size())
        throw mexception(es() % "read \"" % _str_holder(fname) % "\" error");
    return ret;
}

bool read_file(mvector<char>& buf, const char* fname, bool can_empty)
{
    int hfile = ::open(fname, O_RDONLY);
    return read_file_impl(hfile, false, buf, fname, can_empty);
}

bool read_file_and_truncate(mvector<char>& buf, const char* fname, bool can_empty)
{
    int hfile = ::open(fname, O_RDWR);
    if(flock(hfile, LOCK_EX))
        throw_system_failure(es() % "lock file " % _str_holder(fname) % " error");
    return read_file_impl(hfile, true, buf, fname, can_empty);
}

mvector<char> read_file(const char* fname, bool can_empty)
{
    mvector<char> buf;
    read_file(buf, fname, can_empty);
    return buf;
}

void write_file(const char* fname, const char* buf, uint64_t size, bool trunc)
{
    int flags = O_CREAT | O_APPEND | O_RDWR;
    if(trunc)
        flags |= O_TRUNC;
    int hfile = ::open(fname, flags, S_IWRITE | S_IREAD | S_IRGRP | S_IWGRP);
    if(hfile <= 0)
        throw_system_failure(es() % "open file " % _str_holder(fname) % " error");

    uint64_t write_bytes = 0;
    while(write_bytes != size)
    {
        ssize_t w = ::write(hfile, buf + write_bytes, size - write_bytes);
        if(w <= 0)
            break;
        write_bytes += w;
    }
    ::close(hfile);
    if(write_bytes != size)
        throw_system_failure(es() % "write_file(), " % _str_holder(fname) % ", size: " % size);
}

bool get_file_stat(const char* fname, struct stat& st)
{
    int hfile = ::open(fname, O_RDONLY);
    if(hfile < 0)
        return false;

    bool ret = !(::fstat(hfile, &st));
    ::close(hfile);

    if(!ret)
        throw_system_failure(es() % "fstat() error for " % _str_holder(fname));

    return ret;
}

bool is_file_exist(const char* fname, uint64_t* fsize)
{
    struct stat st;
    if(!get_file_stat(fname, st))
        return false;

    int ret = S_ISREG(st.st_mode);
    if(fsize && ret)
        *fsize = st.st_size;

    return ret;
}

uint64_t file_size(const char* fname)
{
    return mfile(fname).size();
}

void remove_file(const char* fname)
{
    if(unlink(fname))
        throw_system_failure(es() % "unlink file error, " % _str_holder(fname));
}

void rename_file(const char* from, const char* to)
{
    if(rename(from, to) == -1)
        throw_system_failure(es() % "rename_file error from " % _str_holder(from) % ", to " % _str_holder(to));
}

bool create_directory(const char* fname)
{
    return !(mkdir(fname, 0777));
}

uint32_t create_directories(const char* fname)
{
    mstring buf(_str_holder(fname));
    mstring::iterator it = (char*)buf.c_str(), ie = buf.end();
    uint32_t ret = 0;
    while(it != ie)
    {
        it = find(it, ie, '/');
        if(it != ie)
        {
            *it = char();
            ret += create_directory(&buf[0]);
            *it = '/';
            ++it;
        }
    }
    ret += create_directory(&buf[0]);
    return ret;
}

bool is_directory(const char* fname)
{
    struct stat st;
    bool ret = get_file_stat(fname, st);
    if(!ret)
        return false;

    return S_ISDIR(st.st_mode);
}

ttime_t get_file_mtime(const char* fname)
{
    struct stat st;
    bool ret = get_file_stat(fname, st);
    if(!ret)
        throw_system_failure(es() % "fstat() error for " % _str_holder(fname));
    return {st.st_mtime * ttime_t::frac};
}

