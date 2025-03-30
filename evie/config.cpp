/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "mlog.hpp"

inline bool is_endl(char c)
{
    return c == '\r' || c == '\n';
}

inline char_cit find_tag(char_cit it_b, char_cit ie, str_holder tag_)
{
    mstring tag = tag_ + " = ";
    char_cit it = it_b;

    for(;;) {
        it = ::search(it, ie, tag.begin(), tag.end());
        if(it == ie)
            return ie;
        bool found = true;

        if(it != it_b) {
            char_cit it_tmp = it - 1;
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
            it = ::find_if(it, ie, &is_endl);
    }
    return it;
}

mstring get_log_name(str_holder fname)
{
    //a.conf, ../a.conf, ~/a.conf
    auto ib = fname.begin(), ie5 = ib + (fname.size() - 5);
    if(fname.size() < 6 || str_holder(ie5, fname.end() - ie5) != ".conf")
        throw str_exception("only *.conf files supported");
    auto it = ie5 - 1;
    for(; it != ib && *it != '/'; --it)
        ;
    if(it != ib)
        ++it;
    return str_holder(it, ie5) + ".log";
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

mvector<str_holder> init_params(int argc, char** argv, bool log_params)
{
    if(log_params)
        print_init(argc, argv);
    mvector<str_holder> ret(argc);
    for(int i = 0; i != argc; ++i)
        ret[i] = _str_holder(argv[i]);
    return ret;
}

str_holder get_config_param_str(char_cit it, char_cit ie, str_holder tag, bool can_empty)
{
    it = find_tag(it, ie, tag);
    char_cit to = ::find_if(it, ie, &is_endl);
    if(it == ie) {
        if(can_empty)
            return str_holder();
        throw mexception(es() % "Load config: \"" % tag % "\" not found in config");
    }
    while(to != it && (*(to - 1) == ' ' || *(to - 1) == '\t'))
        --to;
    {
        char_cit ii = find_tag(to, ie, tag);
        if(ii != ie)
            throw mexception(es() % "Load config: \"" % tag % "\" double usage in config");
    }
    return str_holder(it, to);
}

mvector<mstring> get_config_params(const mvector<char>& cfg, str_holder tag)
{
    mvector<mstring> ret;
    char_cit it = cfg.begin(), ie = cfg.end(), ii;
    while(it != ie) {
        it = find_tag(it, ie, tag);
        if(it != ie) {
            ii = ::find_if(it, ie, &is_endl);
            ret.push_back(mstring(it, ii));
            it = ii;
        }
    }
    return ret;
}

