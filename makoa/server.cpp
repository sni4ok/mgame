/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "server.hpp"
#include "imports.hpp"

#include "evie/thread.hpp"
#include "evie/utils.hpp"
#include "evie/fmap.hpp"

#include <unistd.h>

#include <vector>
#include <thread>

extern bool pooling_mode;

server::impl* server_impl = nullptr;

struct server::impl
{
    volatile bool& can_run;
    bool quit_on_exit;
    std::vector<std::thread> threads;
    fmap<int, pair<hole_importer, void*> > imports;
    my_mutex mutex;
    my_condition cond;

    impl(volatile bool& can_run, bool quit_on_exit) : can_run(can_run), quit_on_exit(quit_on_exit)
    {
        server_impl = this;
    }
    void import_thread(uint32_t& count, mstring str)
    {
        bool need_init = true;
        try
        {
            mstring params = str;
            char* f = params.begin();
            char* c = find(f, params.end(), ' ');
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
                    else
                        usleep(1000 * 1000);
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
    void run(const mvector<mstring>& imports)
    {
        uint32_t count = 0;

        for(mstring i: imports)
        {
            if(i.size() > 7 && str_holder(i.begin(), i.begin() + 7) == "mmap_cp")
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

void server::run(const mvector<mstring>& imports)
{
    pimpl->run(imports);
}

server::server(volatile bool& can_run, bool quit_on_exit)
    : pimpl(new server::impl(can_run, quit_on_exit))
{
}

server::~server()
{
    delete pimpl;
}

