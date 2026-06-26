/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "../makoa/engine.hpp"
#include "../makoa/server.hpp"

#include "../evie/mfile.hpp"
#include "../evie/config.hpp"
#include "../evie/singleton.hpp"
#include "../evie/mlog.hpp"

namespace pip
{
    struct config : stack_singleton<config>
    {
        mstring push, import;
        bool pooling;

        config(char_cit fname)
        {
            auto cs = read_file(fname);
            push = get_config_param<str_holder>(cs, "push");
            import = get_config_param<str_holder>(cs, "import");
            pooling = get_config_param<bool>(cs, "pooling");

            mlog() << "pip_config, import: " << import << ", push: " << push << ", pooling: " << pooling;
        }
    };
}

const str_holder parser_name = "pip";

#define PIP_HERE

#define ns pip

void (*term_signal_func)(int) = server_set_close;

#include "../alco/main.hpp"

void pip::proceed_parser(volatile bool& can_run)
{
    auto& cfg = config::instance();
    engine en(can_run, cfg.pooling, {cfg.push}, 1);
    server sv(can_run, true);
    sv.run({cfg.import});
}
