/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mfile.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

mfile::mfile(const char* file)
{
	hfile = ::open(file, O_RDONLY);
	if(hfile < 0)
		throw_system_failure(es() % "open file " % file % " error");
}

mfile::mfile(int hfile) : hfile(hfile)
{
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

void mfile::read(char* ptr, uint32_t size)
{
	uint32_t read_bytes = ::read(hfile, ptr, size);
	if(read_bytes != size)
		throw_system_failure(es() % "read() error, read_bytes: " % read_bytes % ", required: " % size);
}

mfile::~mfile()
{
	::close(hfile);
}

bool read_file(std::vector<char>& buf, const char* fname, bool can_empty)
{
    bool ret = false;
    uint64_t buf_size = buf.size();
    int hfile = ::open(fname, O_RDONLY);
    if(hfile > 0) {
        mfile f(hfile);
        uint64_t size = f.size();
        buf.resize(buf_size + size);
        f.read(&buf[buf_size], size);
        ret = true;
    }
    if(!can_empty && buf_size == buf.size())
        throw std::runtime_error(es() % "read \"" % _str_holder(fname) % "\" error");
    return ret;
}

std::vector<char> read_file(const char* fname, bool can_empty)
{
    std::vector<char> buf;
    read_file(buf, fname, can_empty);
    return buf;
}

void write_file(const char* fname, const char* buf, uint32_t size, bool trunc)
{
    int flags = O_CREAT | O_APPEND | O_RDWR;
    if(trunc)
        flags |= O_TRUNC;
    int hfile = ::open(fname, flags, S_IWRITE | S_IREAD | S_IRGRP | S_IWGRP);
    if(hfile < 0)
        throw_system_failure(es() % "open file " % _str_holder(fname) % " error");

    uint32_t w = ::write(hfile, buf, size);
    ::close(hfile);

    if(w != size)
        throw_system_failure(es() % "write_file(), " % _str_holder(fname) % ", size: " % size % ", w: " % w);
}

bool is_file_exist(const char* fname, uint64_t* fsize)
{
	int hfile = ::open(fname, O_RDONLY);
	if(hfile < 0)
        return false;

    struct stat st;
	bool ret = !(::fstat(hfile, &st));
    ::close(hfile);

    if(!ret)
		throw_system_failure(es() % "fstat() error for " % fname);

    ret = S_ISREG(st.st_mode);

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
        throw_system_failure(es() % "unlink file error, " % fname);
}

void rename_file(const char* from, const char* to)
{
    if(rename(from, to) == -1)
        throw_system_failure(es() % "rename_file error from " % from % ", to " % to);
}

bool create_directory(const char* fname)
{
    return !(mkdir(fname, 0777));
}

void create_directories(const char* fname)
{
    std::string buf = fname;
    std::string::iterator it = buf.begin(), ie = buf.end();
    while(it != ie)
    {
        it = std::find(it, ie, '/');
        if(it != ie)
        {
            *it = char();
            create_directory(&buf[0]);
            *it = '/';
            ++it;
        }
    }
    create_directory(&buf[0]);
}

bool is_directory(const char* fname)
{
	int hfile = ::open(fname, O_RDONLY);
	if(hfile < 0)
        return false;

    struct stat st;
	bool ret = !(::fstat(hfile, &st));
    ::close(hfile);

    if(!ret)
		throw_system_failure(es() % "fstat() error for " % fname);

    return S_ISDIR(st.st_mode);
}

