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

