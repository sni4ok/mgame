/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "mlog.hpp"
#include "algorithm.hpp"
#include "string.hpp"

inline bool is_endl(char c)
{
    return c == '\r' || c == '\n';
}

inline char_cit find_tag(char_cit ib, char_cit ie, str_holder tag_)
{
    mstring tag = tag_ + " = ";
    char_cit it = ib;

    for(;;)
    {
        it = ::search(it, ie, tag.begin(), tag.end());
        if(it == ie)
            return ie;

        bool found = true;

        if(it != ib)
        {
            char_cit ne = it - 1;
            if(!is_endl(*ne) && *ne != ' ' && *ne != '\t')
                found = false;
            else for(; ne != ib && !is_endl(*ne); --ne)
            {
                if(*ne != ' ' && *ne != '\t')
                {
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
    if(fname.size() < 6 || str_holder(ie5, fname.end()) != ".conf")
        throw str_exception("only *.conf files supported");

    auto it = ie5 - 1;
    for(; it != ib && *it != '/'; --it)
        ;
    if(it != ib)
        ++it;
    return str_holder(it, ie5) + ".log";
}

mvector<str_holder> init_params(int argc, char_cit* argv, bool log_params)
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
    if(it == ie)
    {
        if(can_empty)
            return str_holder();
        throw_exception("get_config_param, not found: ", tag);
    }

    char_cit to = ::find_if(it, ie, &is_endl);
    while(to != it && from_any(*(to - 1), ' ', '\t'))
        --to;

    char_cit ne = find_tag(to, ie, tag);
    if(ne != ie)
        throw_exception("get_config_param, double usage: ", tag);

    return {it, to};
}

mvector<mstring> get_config_params(const mvector<char>& cfg, str_holder tag)
{
    mvector<mstring> ret;
    auto [it, ie] = be(cfg);
    while(it != ie)
    {
        char_cit ne = find_tag(it, ie, tag);
        if(it != ie)
        {
            ne = ::find_if(it, ie, &is_endl);
            ret.push_back({it, ne});
            it = ne;
        }
    }
    return ret;
}

