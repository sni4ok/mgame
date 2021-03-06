/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mfile.hpp"

#include "makoa/types.hpp"

#include <unistd.h>

void fix_zero_tail(const char* fname)
{
    std::cout << "fix_zero_tail(" << fname << "):" << std::endl;
    mfile f(fname);
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
    std::cout << "  orig_file_size: " << fsz << ", new_file_size: " << nsz << std::endl;
    if(truncate(fname, nsz))
        throw std::runtime_error("truncate file error");
}

template<typename type>
void test_io(type c)
{
    char buf[128];
    buf_stream str(buf);
    str << c;
    type v = read_decimal<type>(str.begin(), str.end());
    assert(v == c);
    my_unused(v);
}

void test_impl(const char* a1, const char* a2)
{
    count_t v1 = read_count(a1, a1 + strlen(a1));
    count_t v2 = read_count(a2, a2 + strlen(a2));
    test_io(v1);
    assert(v1.value == v2.value);
    test_io(v1);
    my_unused(v1, v2);
}

void amount_test()
{
    test_impl("-0.5E-50", "0");
    test_impl("5.6", "0.56E1");
    test_impl("0.056", "0.56E-1");
    test_impl("0.56", "5.6E-1");
    test_impl("-0.056", "-0.56E-1");
    test_impl("-5.6", "-0.56E1");
    test_impl("-0.56", "-5.6E-1");
    test_impl("560", "56E1");
    test_impl("5e-7", "0.5E-6");

    const char* v = "9.79380846343861E-4";
    count_t c = read_count(v, v + strlen(v));
    my_unused(c);

    test_io(price_t({std::numeric_limits<int64_t>::max()}));
    test_io(price_t({std::numeric_limits<int64_t>::min()}));

    test_io(count_t({std::numeric_limits<int64_t>::max()}));
    test_io(count_t({std::numeric_limits<int64_t>::min()}));
}

int main(int argc, char** argv)
{
    try
    {
        if(argc == 3 && std::string(argv[1]) == "fix_zero_tail")
            fix_zero_tail(argv[2]);
        else if(argc == 2 && std::string(argv[1]) == "amount_test")
            amount_test();
        else
            throw std::runtime_error("unsupported params");
    }
    catch(std::exception& e)
    {
        std::cerr << "main() exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

