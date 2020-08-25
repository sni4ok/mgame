/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mfile.hpp"

#include "makoa/messages.hpp"

#include <unistd.h>

void fix_zero_tail(const char* fname)
{
    std::cout << "fix_zero_tail(" << fname << "):" << std::endl;
    mfile f(fname, false);
    uint64_t fsz = f.size();
    std::vector<char> m(message_size), mc(message_size);
    int64_t nsz = fsz - fsz % message_size;
    while(nsz > 0) {
        f.seekg(nsz - message_size);
        f.read(&m[0], message_size);
        if(m != mc)
            break;
        nsz -= message_size;
    }
    std:: cout << "  orig_file_size: " << fsz << ", new_file_size: " << nsz << std::endl;
    if(truncate(fname, nsz))
        throw std::runtime_error("truncate file error");
}

int main(int argc, char** argv)
{
    try {
    if(argc == 3 && std::string(argv[1]) == "fix_zero_tail")
        fix_zero_tail(argv[2]);
    else
        throw std::runtime_error("unsupported params");
    }
    catch(std::exception& e) {
        std::cerr << "main() exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

