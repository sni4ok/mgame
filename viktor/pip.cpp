
#include "evie/utils.hpp"
#include "evie/mfile.hpp"
#include "evie/config.hpp"

#include "makoa/exports.hpp"

struct config : stack_singleton<config>
{
    //std::string exchange_id, feed_id;
    std::string push;

    config(const char* fname)
    {
        std::string cs = read_file<std::string>(fname);
        push = get_config_param<std::string>(cs, "push");
    }
};

#include "alco/main.hpp"

struct ifile
{
    std::string fname;
    exporter e;
    mfile file;
    uint64_t file_pos;

    std::vector<char> m;
    char* f, *t;
    const bool realtime;

    ifile(const char* fname, bool realtime) : fname(fname),
        e(config::instance().push), file(fname, true), file_pos(),
        m((1 + 1024 * 1024) * message_size),
        realtime(realtime)
    {
        f = &m[0] + message_size; //for get_export_mtime
        t = &m[0] + m.size();
        ((message*)(&m[0]))->t.time = ttime_t();
    }

    void proceed(volatile bool& can_run)
    {
        bool active = false;
        while(can_run)
        {
            uint64_t cur_sz = file.size();
            if(cur_sz >= file_pos + message_size) {
                uint64_t read_size = std::min<uint64_t>(cur_sz - file_pos, t - f);
                read_size = read_size - (read_size % message_size);
                file.read(f, read_size);
                file_pos += read_size;
                if(active)
                    set_export_mtime((message*)f);
                e.proceed((message*)f, read_size / message_size);
            }
            else if(!realtime) {
                mlog() << "file " << fname << " successfully proceed";
                break;
            }
            else {
                active = true;
            }
        }
    }
};

char** _argv;
int _argc;

void proceed_pip(volatile bool& can_run)
{
    bool realtime = false;

    std::string mode = *_argv;
    if(mode == "realtime")
        realtime = true;
    else if (mode != "history")
        throw std::runtime_error(es() % " unsupported mode: " % mode);

    ifile f(*(_argv + 3), realtime);
    f.proceed(can_run);
}

int main(int argc, char** argv)
{
    if(argc < 4) {
        std::cout << "Usage: ./pip [config_file.conf] mode {mode_params} [date_time_from]" << std::endl;
        return 1;
    }
    std::string s = argv[1];
    if(s.size() > 5) s = std::string(s.end() - 5, s.end());
    bool cf = s == ".conf";
    if(cf) {
        _argv = argv + 2;
        _argc = argc - 2;
    } else {
        _argv = argv + 1;
        _argc = argc - 1;
    }
    return parser_main(cf ? 2 : 1, argv, "pip", proceed_pip);
}


