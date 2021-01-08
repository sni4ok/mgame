/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "mfile.hpp"

inline bool is_endl(char c)
{
    return c == '\r' || c == '\n';
}

template<typename iterator>
iterator found_tag(iterator it_b, iterator it_e, const std::string& tag_)
{
    std::string tag = tag_ + " = ";
    iterator it = it_b;

    for(;;) {
        it = std::search(it, it_e, tag.begin(), tag.end());
        if(it == it_e)
            return it_e;
        bool found = true;

        if(it != it_b) {
            iterator it_tmp = it - 1;
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
            it = std::find_if(it, it_e, &is_endl);
    }
    return it;
}

template<typename type, typename iterator>
type get_config_param(iterator it, iterator it_e, const std::string& tag, bool can_empty = false, type empty_value = type())
{
    it = found_tag(it, it_e, tag);
    iterator it_to = std::find_if(it, it_e, &is_endl);
    if(it == it_e) {
        if(can_empty)
            return empty_value;
        throw std::runtime_error(es() % "Load config: \"" % tag % "\" not found in config");
    }
    while(it_to != it && (*(it_to-1) == ' ' || *(it_to-1) == '\t'))
        --it_to;
    return lexical_cast<type>(std::string(it, it_to));
}

template<typename type, typename buf>
type get_config_param(const buf& cfg, const std::string& tag, bool can_empty = false, type empty_value = type())
{
    return get_config_param<type>(cfg.begin(), cfg.end(), tag, can_empty, empty_value);
}

template<typename type, typename buf>
std::vector<type> get_config_params(const buf& cfg, const std::string& tag)
{
    std::vector<type> ret;
    typename buf::const_iterator it = cfg.begin(), ie = cfg.end(), ii;
    while(it != ie) {
        it = found_tag(it, ie, tag);
        if(it != ie) {
            ii = std::find_if(it, ie, &is_endl);
            ret.push_back(std::string(it, ii));
            it = ii;
        }
    }
    return ret;
}

inline std::string get_log_name(const std::string& fname)
{
    //a.conf, ../a.conf, ~/a.conf
    auto ib = fname.begin(), ie5 = ib + (fname.size() - 5);
    if(fname.size() < 6 || std::string(ie5, fname.end()) != ".conf")
        throw std::runtime_error("only *.conf files supported");
    auto it = ie5 - 1;
    for(; it != ib && *it != '/'; --it)
        ;
    if(it != ib)
        ++it;
    return std::string(it, ie5) + ".log";
}

inline void print_init(int argc, char** argv)
{
    mlog l;
    for(int i = 0; i != argc; ++i) {
        if(i)
            l << " ";
        l << _str_holder(argv[i]);
    }
}

