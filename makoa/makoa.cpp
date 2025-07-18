/*
    This file contains Market Data Engine process
    that collect market data from parsers and translate it to consumers (willo, viktor, ying etc)
    Usage: ./makoa_server [config file]

    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "server.hpp"
#include "config.hpp"
#include "exports.hpp"

#include "../evie/config.hpp"
#include "../evie/mlog.hpp"
#include "../evie/signals.hpp"

int main(int argc, char** argv)
{
    if(argc > 2)
    {
        cout_write("Usage: ./makoa_server [config file]\n");
        return 1;
    }
    auto log = log_init(argc == 1 ? "makoa_server.log" : get_log_name(_str_holder(argv[1])).c_str(),
        mlog::store_tid | mlog::always_cout | mlog::lock_file);

    signals_holder sl(server_set_close);
    mstring name;

    try
    {
        mlog(mlog::always_cout) << "makoa started";
        auto ef = init_efactory();
        print_init(argc, argv);
        config cfg(argc == 1 ? "makoa_server.conf" : argv[1]);
        cfg.print();
        name = cfg.name;
        engine en(can_run, cfg.pooling, cfg.exports, cfg.export_threads, cfg.set_engine_time);
        server sv(can_run);
        sv.run(cfg.imports);
    }
    catch(exception& e)
    {
        mlog(mlog::error | mlog::always_cout) << "makoa(" << name << ") ended with " << e;
        return 1;
    }
    mlog(mlog::always_cout) << "makoa (" << name << ") successfully ended";
    return 0;
}

