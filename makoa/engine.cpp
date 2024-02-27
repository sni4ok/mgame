/*
    This file contains logic for processing market data 
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "exports.hpp"
#include "types.hpp"

#include "evie/thread.hpp"
#include "evie/smart_ptr.hpp"
#include "evie/fast_alloc.hpp"

bool pooling_mode = false;

struct messages
{
    message _;
    message m[255];
    uint32_t count;
    uint32_t cnt; //atomic
};

struct linked_node : messages
{
    linked_node() : next()
    {
    }

    linked_node* next;
};

class linked_list : fast_alloc<linked_node>
{
    typedef fast_alloc<linked_node> base;

    linked_node root;
    linked_node* tail; //atomic
    
public:
    linked_list() : tail(&root)
    {
    }
    void push(linked_node* t) //push element in list, always success
    {
        linked_node* expected = tail;
        while(!atomic_compare_exchange(tail, expected, t))
            expected = tail;

        expected->next = t;
    }
    linked_node* next(linked_node* prev) //can return nullptr, but next time caller should used latest not nullptr value
    {
        if(!prev)
            return root.next;
        return prev->next;
    }
    void release_node(linked_node* n)
    {
        uint32_t consumers_left = atomic_sub(n->cnt, 1);
        if(!consumers_left) {
            n->next = nullptr;
            this->free(n);
        }
    }
    linked_node* alloc()
    {
        linked_node* n = base::alloc();
        n->cnt = 0;
        n->next = nullptr;
        return n;
    }
    void free(linked_node* n)
    {
        base::free(n);
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
        n = nullptr;
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
    mvector<type> data;
    type* last_value;
    type tmp;

public:
    actives() : last_value()
    {
    }
    type& insert(uint32_t security_id)
    {
        tmp = {security_id, ttime_t(), false};
        auto it = lower_bound(data.begin(), data.end(), tmp, less<type>());
        if(unlikely(it != data.end() && it->security_id == security_id)) {
            //if(!it->disconnected)
            //    throw mexception(es() % "activites, security_id " % security_id % " already in active list");
            //else {
                it->disconnected = false;
                it->time = ttime_t(); //TODO: this for several usage makoa_test etc, remove or overthink it later 
            //}
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
        auto it = lower_bound(data.begin(), data.end(), tmp, less<type>());
        if(unlikely(it == data.end() || it->security_id != security_id))
            throw mexception(es() % "activites, security_id " % security_id % " not found in active list");

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
            throw mexception(es() % "context::check() m.time: " % m.time.value % " > time: " % time.value);
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
        mlog() << "~context()";
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
    volatile bool can_exit;
    my_mutex mutex;
    my_condition cond;
    mvector<thread> threads;
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

    struct imple : noncopyable
    {
        volatile bool& can_run;
        linked_list* ll;
        exporter exp;
        linked_node *prev, *ptmp;

        imple(volatile bool& can_run, linked_list& ll, const mstring& eparams) : can_run(can_run), ll(&ll), exp(eparams), prev(), ptmp()
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
                if(!can_run)
                    break;
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

    cas_array<imple, 50> ies;

    void work_thread()
    {
        try
        {
            imple* i = nullptr;
            while(can_run)
            {
                bool res = false;
                i = ies.pop();

                if(i)
                {
                    res = i->proceed();
                    ies.push(i);
                    i = nullptr;
                }
                if(!res) {
                    if(can_exit)
                        break;
                    wait_updates();
                }
            }
            if(i)
                ies.push(i);
        }
        catch(std::exception& e)
        {
            mlog(mlog::error) << "exports: " << " " << e;
        }
    }
    static void log_and_throw_error(const char* data, uint32_t size, str_holder reason)
    {
        mlog() << "bad message (" << reason << "!): " << print_binary((const uint8_t*)data, min<uint32_t>(32, size));
        throw mexception(reason);
    }

public:
    void loop_one()
    {
        /*imple* i = nullptr;
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
            ies.push(i);*/
    }
    impl(volatile bool& can_run) : can_run(can_run), can_exit(false), ies("exporters_queue")
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
    void init(const mvector<mstring>& exports, uint32_t export_threads)
    {
        consumers = exports.size();
        for(const auto& e: exports)
            ies.emplace(can_run, ll, e);

        for(uint32_t i = 0; i != export_threads; ++i)
            threads.push_back(thread(&impl::work_thread, this));
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
                        throw mexception(es() % "instrument crc mismatch, in: " % m->mi.security_id % ", calculated: " % security_id);
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
        can_exit = true;
        for(auto&& t: threads)
            t.join();
    }
    void push_clean(const mvector<actives::type>& secs) //when parser disconnected all OrdersBooks cleans
    {
        uint32_t count = secs.size();
        for(uint32_t ci = 0; ci != count;)
        {
            uint32_t cur_c = min<uint32_t>(count - ci, sizeof(messages::m) / message_size);
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

engine::engine(volatile bool& can_run, bool pooling, const mvector<mstring>& exports, uint32_t export_threads) : pimpl()
{
    set_can_run(&can_run);
    pooling_mode = pooling;
    unique_ptr<engine::impl> p(new engine::impl(can_run));
    p->init(exports, export_threads);
    pimpl = p.release();
}

engine::~engine()
{
    delete pimpl;
}

void actives::on_disconnect()
{
    mlog() << "makoa() actives::on_disconnect";
    engine::impl::instance().push_clean(data);
    auto it = data.begin(), ie = data.end();
    for(; it != ie; ++it)
    {
        auto& v = get(it->security_id);
        if(v.disconnected)
            mlog(mlog::warning) << "makoa() actives::on_disconnect(), " << it->security_id << " already disconnected";

        get(it->security_id).disconnected = true;
    }
}

pair<void*, str_holder> import_context_create(void*)
{
    return {new context(), engine::impl::instance().alloc()};
}

void import_context_destroy(pair<void*, str_holder> ctx)
{
    engine::impl::instance().free(ctx.second, (context*)(ctx.first));
    delete (context*)ctx.first;
}

bool import_proceed_data(str_holder& buf, void* ctx)
{
    //MPROFILE("proceed_data")
    return engine::impl::instance().proceed(buf, (context*)(ctx));
}

