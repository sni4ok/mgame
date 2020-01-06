/*
    This file contains logic for processing market data 
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "exports.hpp"
#include "types.hpp"

#include "evie/fast_alloc.hpp"
#include "evie/mlog.hpp"
#include "evie/time.hpp"

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>

struct messages
{
    messages()
    {
    }
    ttime_t mtime;
    message m[50];
    uint32_t count;
    std::atomic<uint32_t> cnt;
};

struct linked_node : messages
{
    linked_node() : next()
    {
    }
    linked_node* next;
};

class linked_list : public fast_alloc<linked_node, 4 * 1024>
{
    linked_node root;
    std::atomic<type*> tail;
    
    std::mutex& mutex;
    std::condition_variable& cond;

public:
    linked_list(std::mutex& mutex, std::condition_variable& cond) : tail(&root), mutex(mutex), cond(cond)
    {
    }
    void push(linked_node* t) //push element in list, always success
    {
        type* expected = tail;
        while(!tail.compare_exchange_weak(expected, t)) {
            expected = tail;
        }
        expected->next = t;

        bool lock = mutex.try_lock();
        cond.notify_all();
        if(lock)
            mutex.unlock();
    }
    linked_node* next(linked_node* prev) //can return nullptr, but next time caller should used latest not nullptr value
    {
        if(!prev)
            return root.next;
        return prev->next;
    }
};

struct actives : noncopyable
{
    struct type
    {
        uint32_t security_id;
        ttime_t time; //last parser time for current security_id
        bool disconnected;
        bool operator < (const type& r) const {
            return security_id < r.security_id;
        }
    };

private:
    std::vector<type> data;
    type* last_value;
    type tmp;

public:
    actives() : last_value()
    {
    }
    type& insert(uint32_t security_id)
    {
        tmp = {security_id, ttime_t(), false};
        auto it = std::lower_bound(data.begin(), data.end(), tmp);
        if(unlikely(it != data.end() && it->security_id == security_id)) {
            if(!it->disconnected)
                throw std::runtime_error(es() % "activites, security_id " % security_id % " already in active list");
            else {
                it->disconnected = false;
                it->time = ttime_t(); //TODO: this for several usage makoa_test etc, remove or overthink it later 
            }
        } else {
            it = data.insert(it, tmp);
        }
        last_value = &(*it);
        return *last_value;
    }
    type& get(uint32_t security_id)
    {
        if(last_value && last_value->security_id == security_id)
            return *last_value;

        tmp.security_id = security_id;
        auto it = std::lower_bound(data.begin(), data.end(), tmp);
        if(unlikely(it == data.end() || it->security_id != security_id))
            throw std::runtime_error(es() % "activites, security_id " % security_id % " not found in active list");

        last_value = &(*it);
        return *last_value;
    }
    void on_disconnect();
};

struct context
{
    actives acs;

    uint32_t buf_delta;
    context() : buf_delta()
    {
    }
    void insert(uint32_t security_id, ttime_t time)
    {
        acs.insert(security_id).time = time;
    }
    actives::type& check(uint32_t security_id, ttime_t time)
    {
        auto& m = acs.get(security_id);
        if(unlikely(m.time > time))
            throw std::runtime_error(es() % "context::check() m.time: " % m.time % " > time: " % time);
        m.time = time;
        return m;
    }
    void check_clean(const message_clean& mc)
    {
        auto& v = check(mc.security_id, mc.time);
        if(mc.source == 1)
            v.disconnected = true;
    }
    ~context()
    {
        acs.on_disconnect();
    }
};

class engine::impl : public stack_singleton<engine::impl>
{
    volatile bool can_run;
    std::mutex mutex;
    std::condition_variable cond;
    std::vector<std::thread> threads; //consume workers, one thread for each exporter
    linked_list ll;
    uint32_t consumers;

    void work_thread(exporter exp)
    {
        try{
            linked_list::type *prev = nullptr, *ptmp;
            while(can_run)
            {
                //lockfree when queue not empty
                ptmp = ll.next(prev);
        repeat:
                if(ptmp){
                    exp.proceed(ptmp->m, ptmp->count);
                    if(prev){
                        //we release prev here
                        uint32_t consumers_left = --(prev->cnt);
                        if(!consumers_left){
                            prev->next = nullptr;
                            ll.free(prev);
                        }
                    }
                    prev = ptmp;
                }
                else{
                    //here queue is empty, so wait on mutex for new messages
                    bool lock = mutex.try_lock();
                    if(lock) {
                        std::unique_lock<std::mutex> lock(mutex, std::adopt_lock_t());
                        ptmp = ll.next(prev);
                        if(ptmp)
                            goto repeat;
                        else
                            cond.wait_for(lock, std::chrono::microseconds(100 * 1000));
                    }
                }
            }
        }
        catch(std::exception& e){
            mlog(mlog::error) << "exports: " << " " << e;
            //TODO: close connection for market data provider for current message
            //this should be done in server::impl::work_thread
        }
    }
    static void log_and_throw_error(const char* data, uint32_t size, const char* reason)
    {
        mlog() << "bad message (" << str_holder(reason) << "!): " << print_binary((const uint8_t*)data, std::min<uint32_t>(32, size));
        throw std::runtime_error("bad message");
    }
public:
    impl() : can_run(true), ll(mutex, cond)
    {
    }
    str_holder alloc()
    {
        linked_node* p = ll.alloc();
        return str_holder((const char*)p->m, sizeof(p->m));
    }
    void free(str_holder buf, context* ctx)
    {
        const char* m = (buf.str - ctx->buf_delta - sizeof(messages::mtime));
        ll.free((linked_node*)m);
    }
    void init()
    {
        for(const auto& m: config::instance().exports)
            threads.push_back(std::thread(&impl::work_thread, this, exporter(m)));
        consumers = threads.size();
    }
    void proceed(str_holder& buf, context* ctx)
    {
        //MPROFILE("engine::proceed()")
        //mlog() << "proceed " << size << " bytes";
        //uint32_t consumed = 0;

        uint32_t full_size = buf.size + ctx->buf_delta;
        uint32_t count = full_size / message_size;
        uint32_t cur_delta = full_size % message_size;
        ttime_t mtime = get_cur_ttime();

        const char* ptr = buf.str - ctx->buf_delta;
        message* m = (message*)(ptr);
        linked_node* n = (linked_node*)(ptr - sizeof(messages::mtime));
        n->mtime = mtime;
        n->count = count;
        n->cnt = consumers + 1;
        ll.push(n);
        cond.notify_all();
        
        struct node_free
        {
            linked_node* n;
            linked_list& ll;
            node_free(linked_node* n, linked_list& ll) : n(n), ll(ll) 
            {
            }
            ~node_free() {
                uint32_t consumers_left = --(n->cnt);
                if(!consumers_left){
                    n->next = nullptr;
                    ll.free(n);
                }
            }
        };
        node_free nf(n, ll);

        linked_node *e = ll.alloc();
        buf.size = sizeof(messages::m) - cur_delta;
        buf.str = (const char*)(e->m) + cur_delta;
        if(cur_delta)
            memcpy(e->m, ptr + count * message_size, cur_delta);
        ctx->buf_delta = cur_delta;

        for(uint32_t i = 0; i != count; ++i, ++m)
        {
            switch(m->id.id) {
                case(msg_book) : {
                    ctx->check(m->mb.security_id, m->mb.time);
                    break;
                }
                case(msg_trade) : {
                    ctx->check(m->mt.security_id, m->mt.time);
                    break;
                }
                case(msg_clean) : {
                    ctx->check_clean(m->mc);
                    break;
                }
                case(msg_instr) : {
                    uint32_t security_id = calc_crc(m->mi);
                    if(security_id != m->mi.security_id)
                        throw std::runtime_error(es() % "instrument crc mismatch, in: " % m->mi.security_id % ", calculated: " % security_id);
                    ctx->insert(security_id, m->mi.time);
                    break;
                }
                case(msg_ping) : {
                    break;
                }
                case(msg_hello) : {
                    //mlog() << "<hello|" << t->name << "|" << t->time << "|";
                    break;
                }
                default:
                    log_and_throw_error(ptr, full_size, es() % "bad msg_id: " % m->id.id);
                
            }
        }
    }
    ~impl()
    {
        can_run = false;
        for(auto&& t: threads)
            t.join();
    }
    void push_clean(const std::vector<actives::type>& secs) //when parser disconnected all OrdersBooks cleans
    {
        uint32_t count = secs.size();
        ttime_t mtime = get_cur_ttime();
           
        for(uint32_t ci = 0; ci != count;)
        {
            uint32_t cur_c = std::min<uint32_t>(count - ci, sizeof(messages::m) / message_size);
            linked_node* n = ll.alloc();
            n->mtime = mtime;
            n->count = cur_c;
            n->cnt = consumers;
            for(uint32_t i = 0; i != cur_c; ++i, ++ci)
                n->m[i].mc = message_clean{secs[ci].time, ttime_t(), msg_clean, "", secs[ci].security_id, 1/*source*/};
            ll.push(n);
        }
    }
};

engine::engine()
{
    pimpl = std::make_unique<engine::impl>();
    pimpl->init();
}

engine::~engine()
{
}

void actives::on_disconnect()
{
    mlog() << "actives::on_disconnect";
    engine::impl::instance().push_clean(data);
    auto it = data.begin(), ie = data.end();
    for(; it != ie; ++it) {
        auto& v = get(it->security_id);
        if(v.disconnected)
            mlog(mlog::warning) << "actives::on_disconnect(), " << it->security_id << " already disconnected";

        get(it->security_id).disconnected = true;
    }
}

context_holder::context_holder()
{
    ctx = new context();
}

context_holder::~context_holder()
{
    delete ctx;
}

str_holder alloc_buffer()
{
    return engine::impl::instance().alloc();
}

void free_buffer(str_holder buf, context* ctx)
{
    engine::impl::instance().free(buf, ctx);
}

void proceed_data(str_holder& buf, context* ctx)
{
    return engine::impl::instance().proceed(buf, ctx);
}

