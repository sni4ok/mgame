/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "server.hpp"
#include "imports.hpp"

#include "evie/mutex.hpp"
#include "evie/utils.hpp"

#include <unistd.h>

extern bool pooling_mode;

struct server::impl : stack_singleton<server::impl>
{
    volatile bool& can_run;
    bool quit_on_exit;
    std::vector<std::thread> threads;
    std::vector<std::pair<hole_importer, void*> > imports;
    my_mutex mutex;

    impl(volatile bool& can_run, bool quit_on_exit) : can_run(can_run), quit_on_exit(quit_on_exit)
    {
    }
    void import_thread(volatile int& init, std::string str)
    {
        try
        {
            std::string params = str;
            char* f = (char*)params.c_str();
            char* c = (char*)std::find(f, f + params.size(), ' ');
            *c = char();
            hole_importer hi = create_importer(f);
            void* i = hi.init(can_run, c + 1);
            init = 1;
            {
                my_mutex::scoped_lock lock(mutex);
                imports.push_back(std::make_pair(hi, i));
            }
            while(can_run) {
                try {
                    hi.start(i, nullptr);
                    if(quit_on_exit)
                        break;
                } catch (std::exception& e) {
                    mlog() << "import_thread " << str << " " << e;
                    for(uint i = 0; can_run && i != 5; ++i)
                        sleep(1);
                }
            }

            hi.destroy(i);
        }
        catch(std::exception& e) {
            can_run = false;
            mlog() << "import_thread ended, " << str << " " << e;
        }
    }
    void run(const std::vector<std::string>& imports)
    {
        std::vector<int> inits(imports.size());
        uint32_t idx = 0;

        for(std::string i: imports)
        {
            if(i.size() > 7 && std::equal(i.begin(), i.begin() + 7, "mmap_cp"))
                i = i + (pooling_mode ? " 1" : " 0");

            threads.push_back(std::thread(&impl::import_thread, this, std::ref(inits[idx++]), i));
        }

        for(uint32_t started = 0, cnt = 0; started != inits.size(); ++cnt)
        {
            for(int v: inits)
                started += v;

            if(started != inits.size()) {
                if(cnt == 30)
                    throw std::runtime_error("server::run init imports fails");
                sleep(1);
            }
        }
    }
    ~impl()
    {
        for(auto& i: imports) {
            if(i.first.set_close)
                i.first.set_close(i.second);
        }
        for(auto&& t: threads)
            t.join();
    }
};

void server::run(const std::vector<std::string>& imports)
{
    pimpl->run(imports);
}
server::server(volatile bool& can_run, bool quit_on_exit)
{
    pimpl = std::make_unique<server::impl>(can_run, quit_on_exit);
}
server::~server()
{
}

