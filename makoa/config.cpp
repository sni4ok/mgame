/*
    config for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "../evie/config.hpp"
#include "../evie/mfile.hpp"
#include "../evie/mlog.hpp"

config::config(char_cit fname)
{
    auto cs = read_file(fname);
    name = get_config_param<str_holder>(cs, "name");

    imports = get_config_params(cs, "import");
    exports = get_config_params(cs, "export");
    export_threads = get_config_param<uint32_t>(cs, "export_threads");

    pooling = get_config_param<bool>(cs, "pooling");
    set_engine_time = get_config_param<bool>(cs, "set_engine_time");
}

void config::print()
{
    mlog ml;
    ml << "config params:\n"
        << "  name: " << name << "\n";
    ml << "  imports:\n";
    for(auto v: imports)
        ml << "      " << v << "\n";
    ml << "  exports:\n";
    for(auto v: exports)
        ml << "      " << v << "\n";
    ml << "  export_threads: " << export_threads << "\n"
        << "  pooling: " << pooling << ", set_engine_time: " << set_engine_time << "\n";
}

