/*
    This file contains logic for processing market data 
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "exports.hpp"
#include "types.hpp"
#include "config.hpp"

#include "evie/fast_alloc.hpp"

bool pooling_mode = false;

struct messages
{
    messages() : cnt()
    {
    }

    message _;
    message m[255];
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
    std::atomic<linked_node*> tail;
    
public:
    linked_list() : fast_alloc("messages_linked_list"), tail(&root)
    {
    }
    void push(linked_node* t) //push element in list, always success
    {
        linked_node* expected = tail;
        while(!tail.compare_exchange_weak(expected, t))
            expected = tail;

        expected->next = t;
    }
    linked_node* next(linked_node* prev) //can return nullptr, but next time caller should used latest not nullptr value
    {
        if(!prev)
            return root.next;
        return prev->next;
    }
    void release_node(linked_list::type* node)
    {
        uint32_t consumers_left = --(node->cnt);
        if(!consumers_left)
        {
            node->next = nullptr;
            this->free(node);
        }
    }
};

struct node_free
{
    linked_node* n;
    linked_list& ll;

    node_free(linked_node* n, linked_list& ll) : n(n), ll(ll) 
    {
    }
    void release()
    {
        ll.release_node(n);
        n = 0;
    }
    ~node_free()
    {
        try {
            if(n)
                ll.release_node(n);
        }
        catch(std::exception& e) {
            mlog() << "~node_free() " << e;
        }
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
            throw std::runtime_error(es() % "context::check() m.time: " % m.time.value % " > time: " % time.value);
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
        try
        {
            acs.on_disconnect();
        }
        catch(std::exception& e)
        {
            mlog() << "~context() " << e;
        }
    }
};

class engine::impl : public stack_singleton<engine::impl>
{
    volatile bool& can_run;
    my_mutex mutex;
    my_condition cond;
    std::vector<std::thread> threads;
    linked_list ll;
    uint32_t consumers;

    void notify()
    {
        if(!pooling_mode)
        {
            //MPROFILE("notify_lock")
            bool lock = mutex.try_lock();
            cond.notify_all();
            if(lock)
                mutex.unlock();
        }
    }
    void wait_updates()
    {
        if(!pooling_mode)
        {
            //MPROFILE("wait_lock()")
            my_mutex::scoped_lock lock(mutex);
            cond.timed_uwait(lock, 100 * 1000);
        }
    }

    struct imple
    {
        linked_list* ll;
        exporter exp;
        linked_list::type *prev, *ptmp;

        imple()
        {
        }
        imple(linked_list& ll, const std::string& eparams) : ll(&ll), exp(eparams), prev(), ptmp()
        {
        }
        bool proceed()
        {
            bool ret = false;
            ptmp = ll->next(prev);
            while(ptmp)
            {
                exp.proceed(ptmp->m, ptmp->count);
                ret = true;
                if(prev)
                    ll->release_node(prev);
                prev = ptmp;
                ptmp = ll->next(prev);
            }
            return ret;
        }
        ~imple()
        {
            try {
                if(prev)
                    ll->release_node(prev);
            }
            catch(std::exception& e) {
                mlog() << "~imple() " << e;
            }
        }
    };

    lockfree_queue<imple*, 50> ies;

    void work_thread()
    {
        try
        {
            imple* i = nullptr;
            while(can_run)
            {
                bool res = false;
                ies.pop_weak(i);

                if(i)
                {
                    res = i->proceed();
                    ies.push(i);
                    i = 0;
                }
                if(!res)
                    wait_updates();
            }
            if(i)
                ies.push(i);
        }
        catch(std::exception& e)
        {
            mlog(mlog::error) << "exports: " << " " << e;
        }
    }
    static void log_and_throw_error(const char* data, uint32_t size, const std::string& reason)
    {
        mlog() << "bad message (" << reason << "!): " << print_binary((const uint8_t*)data, std::min<uint32_t>(32, size));
        throw std::runtime_error(reason);
    }

public:
    void loop_one()
    {
        imple* i = nullptr;
        for(uint32_t c = 0; c != ies.capacity && can_run; ++c)
        {
            bool res = false;
            ies.pop_weak(i);
            if(i)
            {
                res = i->proceed();
                ies.push(i);
                i = 0;
            }
            if(!res)
                return;
        }
        if(i)
            ies.push(i);
    }
    impl(volatile bool& can_run) : can_run(can_run), ies("exporters_queue")
    {
    }
    str_holder alloc()
    {
        linked_node* p = ll.alloc();
        return str_holder((const char*)p->m, sizeof(p->m));
    }
    void free(str_holder buf, context* ctx)
    {
        const char* m = (buf.str - ctx->buf_delta - sizeof(messages::_));
        ll.free((linked_node*)m);
    }
    void init()
    {
        consumers = config::instance().exports.size();
        for(const auto& e: config::instance().exports)
            ies.push(new imple(ll, e));

        for(uint32_t i = 0; i != config::instance().export_threads; ++i)
            threads.push_back(std::thread(&impl::work_thread, this));
    }
    bool proceed(str_holder& buf, context* ctx)
    {
        //MPROFILE("engine::proceed()")

        uint32_t full_size = buf.size + ctx->buf_delta;
        uint32_t count = full_size / message_size;
        uint32_t cur_delta = full_size % message_size;

        const char* ptr = buf.str - ctx->buf_delta;
        message* m = (message*)(ptr);
        linked_node* n = (linked_node*)(ptr - sizeof(linked_node::_));
        set_export_mtime(m);
        n->count = count;
        n->cnt = consumers + 1;
        ll.push(n);
        notify();
        
        node_free nf(n, ll);

        linked_node *e = ll.alloc();
        buf.size = sizeof(messages::m) - cur_delta;
        buf.str = (const char*)(e->m) + cur_delta;
        if(cur_delta)
            memcpy(e->m, ptr + count * message_size, cur_delta);
        ctx->buf_delta = cur_delta;
        
        //if(!cur_delta)
        //    loop_one();

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
        nf.release();
        return true;
    }
    ~impl()
    {
        for(auto&& t: threads)
            t.join();
        imple *i = nullptr;
        while(ies.pop_weak(i))
            delete i;
    }
    void push_clean(const std::vector<actives::type>& secs) //when parser disconnected all OrdersBooks cleans
    {
        uint32_t count = secs.size();
        for(uint32_t ci = 0; ci != count;)
        {
            uint32_t cur_c = std::min<uint32_t>(count - ci, sizeof(messages::m) / message_size);
            linked_node* n = ll.alloc();
            set_export_mtime(n->m);
            n->count = cur_c;
            n->cnt = consumers;
            for(uint32_t i = 0; i != cur_c; ++i, ++ci)
                n->m[i].mc = message_clean{secs[ci].time, ttime_t(), msg_clean, "", secs[ci].security_id, 1/*source*/};
            ll.push(n);
            notify();
        }
    }
};

engine::engine(volatile bool& can_run)
{
    pooling_mode = config::instance().pooling;
    pimpl = std::make_unique<engine::impl>(can_run);
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
    for(; it != ie; ++it)
    {
        auto& v = get(it->security_id);
        if(v.disconnected)
            mlog(mlog::warning) << "actives::on_disconnect(), " << it->security_id << " already disconnected";

        get(it->security_id).disconnected = true;
    }
}

void* import_context_create(void*)
{
    return (void*)(new context());
}

void import_context_destroy(void* ctx)
{
    delete (context*)ctx;
}

str_holder import_alloc_buffer(void*)
{
    return engine::impl::instance().alloc();
}

void import_free_buffer(str_holder buf, void* ctx)
{
    engine::impl::instance().free(buf, (context*)(ctx));
}

bool import_proceed_data(str_holder& buf, void* ctx)
{
    //MPROFILE("proceed_data")
    return engine::impl::instance().proceed(buf, (context*)(ctx));
}

