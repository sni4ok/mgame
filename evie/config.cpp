/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "mlog.hpp"

bool is_endl(char c)
{
    return c == '\r' || c == '\n';
}

char_cit found_tag(char_cit it_b, char_cit it_e, str_holder tag_)
{
    mstring tag = mstring(tag_) + " = ";
    char_cit it = it_b;

    for(;;) {
        it = ::search(it, it_e, tag.begin(), tag.end());
        if(it == it_e)
            return it_e;
        bool found = true;

        if(it != it_b) {
            const char* it_tmp = it - 1;
            if(!is_endl(*it_tmp) && *it_tmp != ' ' && *it_tmp != '\t')
                found = false;
            else for(; it_tmp != it_b && !is_endl(*it_tmp); --it_tmp) {
                if(*it_tmp != ' ' && *it_tmp != '\t') {
                    found = false;
                    break;
                }
            }
        }
        if(found)
            return it + tag.size();
        else
            it = ::find_if(it, it_e, &is_endl);
    }
    return it;
}

mstring get_log_name(const mstring& fname)
{
    //a.conf, ../a.conf, ~/a.conf
    auto ib = fname.begin(), ie5 = ib + (fname.size() - 5);
    if(fname.size() < 6 || str_holder(ie5, fname.end() - ie5) != ".conf")
        throw mexception("only *.conf files supported");
    auto it = ie5 - 1;
    for(; it != ib && *it != '/'; --it)
        ;
    if(it != ib)
        ++it;
    return mstring(it, ie5) + ".log";
}

void print_init(int argc, char** argv)
{
    mlog l(mlog::no_cout);
    for(int i = 0; i != argc; ++i) {
        if(i)
            l << " ";
        l << _str_holder(argv[i]);
    }
}

mvector<mstring> init_params(int argc, char** argv, bool log_params)
{
    if(log_params)
        print_init(argc, argv);
    mvector<mstring> ret(argc);
    for(int i = 0; i != argc; ++i)
        ret[i] = mstring(argv[i]);
    return ret;
}

mvector<mstring> get_config_params(const mvector<char>& cfg, str_holder tag)
{
    mvector<mstring> ret;
    const char* it = cfg.begin(), *ie = cfg.end(), *ii;
    while(it != ie) {
        it = found_tag(it, ie, tag);
        if(it != ie) {
            ii = ::find_if(it, ie, &is_endl);
            ret.push_back(mstring(it, ii));
            it = ii;
        }
    }
    return ret;
}

