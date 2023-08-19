/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "server.hpp"
#include "imports.hpp"

#include "evie/mutex.hpp"
#include "evie/utils.hpp"
#include "evie/fmap.hpp"

#include <unistd.h>

extern bool pooling_mode;

server::impl* server_impl = nullptr;

struct server::impl
{
    volatile bool& can_run;
    bool quit_on_exit;
    std::vector<std::thread> threads;
    fmap<int, std::pair<hole_importer, void*> > imports;
    my_mutex mutex;
    my_condition cond;

    impl(volatile bool& can_run, bool quit_on_exit) : can_run(can_run), quit_on_exit(quit_on_exit)
    {
        server_impl = this;
    }
    void import_thread(uint32_t& count, std::string str)
    {
        bool need_init = true;
        try
        {
            std::string params = str;
            char* f = (char*)params.c_str();
            char* c = (char*)std::find(f, f + params.size(), ' ');
            *c = char();
            hole_importer hi = create_importer(f);
            void* i = hi.init(can_run, c + 1);
            {
                my_mutex::scoped_lock lock(mutex);
                imports[get_thread_id()] = {hi, i};
                ++count;
                need_init = false;
                cond.notify_all();
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
        catch(std::exception& e)
        {
            can_run = false;
            mlog() << "import_thread ended, " << str << " " << e;
        }
        my_mutex::scoped_lock lock(mutex);
        if(need_init)
            ++count;
        else
            imports.erase(get_thread_id());
        cond.notify_all();
    }
    void run(const std::vector<std::string>& imports)
    {
        uint32_t count = 0;

        for(std::string i: imports)
        {
            if(i.size() > 7 && std::equal(i.begin(), i.begin() + 7, "mmap_cp"))
                i = i + (pooling_mode ? " 1" : " 0");

            threads.push_back(std::thread(&impl::import_thread, this, std::ref(count), i));
        }
        while(count != imports.size())
        {
            my_mutex::scoped_lock lock(mutex);
            cond.wait(lock);
        }
    }
    void set_close()
    {
        my_mutex::scoped_lock lock(mutex);
        for(auto& i: imports) {
            if(i.second.first.set_close)
                i.second.first.set_close(i.second.second);
        }
    }
    ~impl()
    {
        for(auto&& t: threads)
            t.join();
        server_impl = nullptr;
    }
};

void server_set_close()
{
    if(server_impl)
        server_impl->set_close();
}

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

