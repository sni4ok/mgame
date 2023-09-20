/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "string.hpp"
#include "mstring.hpp"
#include "algorithm.hpp"

bool is_endl(char c);
char_cit found_tag(char_cit it_b, char_cit it_e, str_holder tag);

mstring get_log_name(const mstring& fname);
void print_init(int argc, char** argv);
mvector<mstring> init_params(int argc, char** argv, bool log_params = true);
mvector<mstring> get_config_params(const mvector<char>& cfg, str_holder tag);

template<typename type>
type get_config_param(char_cit it, char_cit it_e, str_holder tag, bool can_empty = false, type empty_value = type())
{
    it = found_tag(it, it_e, tag);
    const char* it_to = ::find_if(it, it_e, &is_endl);
    if(it == it_e) {
        if(can_empty)
            return empty_value;
        throw mexception(es() % "Load config: \"" % tag % "\" not found in config");
    }
    while(it_to != it && (*(it_to-1) == ' ' || *(it_to-1) == '\t'))
        --it_to;
    return lexical_cast<type>(it, it_to);
}

template<typename type>
type get_config_param(const mvector<char>& cfg, str_holder tag, bool can_empty = false, type empty_value = type())
{
    return get_config_param<type>(cfg.begin(), cfg.end(), tag, can_empty, empty_value);
}

