/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "config.hpp"
#include "server.hpp"
#include "imports.hpp"

#include <mutex>
#include <thread>
#include <vector>

#include <unistd.h>

extern bool pooling_mode;

struct server::impl : stack_singleton<server::impl>
{
    volatile bool& can_run;
    std::vector<std::thread> threads;
    std::vector<std::pair<hole_importer, void*> > imports;
    std::mutex mutex;

    impl(volatile bool& can_run) : can_run(can_run)
    {
    }
    void import_thread(std::string params)
    {
        try
        {
            char* f = (char*)params.c_str();
            char* c = (char*)std::find(f, f + params.size(), ' ');
            *c = char();
            hole_importer hi = create_importer(f);
            void* i = hi.init(can_run, c + 1);
            {
                std::unique_lock<std::mutex> lock(mutex);
                imports.push_back(std::make_pair(hi, i));
            }
            while(can_run) {
                try {
                    hi.start(i);
                } catch (std::exception& e) {
                    mlog() << "import_thread " << params << " " << e;
                    for(uint i = 0; can_run && i != 5; ++i)
                        sleep(1);
                }
            }

            hi.destroy(i);
        }
        catch(std::exception& e) {
            can_run = false;
            mlog() << "import_thread ended, " << params << " " << e;
        }
    }
    void run()
    {
        for(std::string i: config::instance().imports)
        {
            if(i.size() > 7 && std::equal(i.begin(), i.begin() + 7, "mmap_cp"))
                i = i + (pooling_mode ? " 1" : " 0");

            threads.push_back(std::thread(&impl::import_thread, this, i));
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

void server::import_loop()
{
    pimpl->run();
}
server::server(volatile bool& can_run)
{
    pimpl = std::make_unique<server::impl>(can_run);
}
server::~server()
{
}

