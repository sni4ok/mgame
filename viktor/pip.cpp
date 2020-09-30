/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mfile.hpp"
#include "evie/config.hpp"

#include "makoa/exports.hpp"
#include "makoa/imports.hpp"

struct config : stack_singleton<config>
{
    std::string push, import;

    config(const char* fname)
    {
        std::string cs = read_file<std::string>(fname);
        push = get_config_param<std::string>(cs, "push");
        import = get_config_param<std::string>(cs, "import");

        mlog() << "config() import: " << import << ", push: " << push;
    }
};

#include "alco/main.hpp"

void* import_context_create(void*)
{
    exporter* e = new exporter(config::instance().push);
    return (void*)e;
}
void import_context_destroy(void* e)
{
    delete (exporter*)e;
}

char msg_buf[message_size * 256];

str_holder import_alloc_buffer(void*)
{
    return str_holder(msg_buf + 1, 255 * message_size);
}

void import_free_buffer(str_holder, void*)
{
}

void import_proceed_data(str_holder& buf, void* ctx)
{
    exporter* e = (exporter*)ctx;
    message* m = (message*)buf.str;
    uint32_t count = buf.size / message_size;
    if(unlikely(buf.size % message_size))
        throw std::runtime_error("fractional messages not supported yet");
    e->proceed(m, count);
    buf.size = 255 * message_size;
}

void proceed_pip(volatile bool& can_run)
{
    char* f = (char*)config::instance().import.c_str();
    char* c = (char*)std::find(f, f + config::instance().import.size(), ' ');
    *c = char();
    hole_importer hi = create_importer(f);
    void* i = hi.init(can_run, c + 1);
    hi.start(i, nullptr);
    hi.destroy(i);
}

int main(int argc, char** argv)
{
    return parser_main(argc, argv, "pip", proceed_pip);
}

