/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "mfile.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

mfile::mfile(const char* file, bool direct)
{
	hfile = ::open(file, direct ? O_DIRECT | O_RDONLY : O_RDONLY);
	if(hfile < 0)
		throw_system_failure(es() % "open file " % file % " error");
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

