/*
    config for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"

#include "evie/mfile.hpp"

config::config(const char* fname)
{
    std::string cs = read_file<std::string>(fname);
    log_exporter = get_config_param<bool>(cs, "log_exporter", true, false);
    port = get_config_param<uint16_t>(cs, "port");
    name = get_config_param<std::string>(cs, "name");

    std::string e = get_config_param<std::string>(cs, "exports", true);
    exports = split(e);
    auto ex = get_config_params<std::string>(cs, "export");
    std::copy(ex.begin(), ex.end(), std::back_inserter(exports));
}

void config::print()
{
    mlog ml;
    ml << "config params:\n  log_exporter: " << log_exporter << "\n"
        << "  port: " << port << "\n"
        << "  name: " << name << "\n";
    if(!exports.empty()) {
        ml << "  exports:\n";
        for(auto v: exports)
            ml << "      " << v << "\n";
    }
}

