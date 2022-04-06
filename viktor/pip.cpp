/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mfile.hpp"
#include "evie/config.hpp"

#include "makoa/engine.hpp"
#include "makoa/server.hpp"

struct config : stack_singleton<config>
{
    std::string push, import;

    config(const char* fname)
    {
        auto cs = read_file(fname);
        push = get_config_param<std::string>(cs, "push");
        import = get_config_param<std::string>(cs, "import");

        mlog() << "config() import: " << import << ", push: " << push;
    }
};

#include "alco/main.hpp"

void proceed_pip(volatile bool& can_run)
{
    engine en(can_run, false, {config::instance().push}, 1);
    server sv(can_run, true);
    sv.run({config::instance().import});
}

int main(int argc, char** argv)
{
    return parser_main(argc, argv, "pip", proceed_pip);
}

