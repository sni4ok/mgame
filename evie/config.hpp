/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "string.hpp"
#include "mstring.hpp"
#include "algorithm.hpp"

mstring get_log_name(const mstring& fname);
void print_init(int argc, char** argv);
mvector<str_holder> init_params(int argc, char** argv, bool log_params = true);
str_holder get_config_param_str(char_cit it, char_cit it_e, str_holder tag, bool can_empty);
mvector<mstring> get_config_params(const mvector<char>& cfg, str_holder tag);

template<typename type>
type get_config_param(char_cit it, char_cit it_e, str_holder tag, bool can_empty = false, type empty_value = type())
{
    str_holder s = get_config_param_str(it, it_e, tag, can_empty);
    if(can_empty && s.size == 0)
        return empty_value;
    return lexical_cast<type>(s);
}

template<typename type>
type get_config_param(const mvector<char>& cfg, str_holder tag, bool can_empty = false, type empty_value = type())
{
    return get_config_param<type>(cfg.begin(), cfg.end(), tag, can_empty, empty_value);
}

