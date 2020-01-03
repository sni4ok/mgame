/*
    This file contains logic for processing market data 
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "engine.hpp"
#include "exports.hpp"
#include "securities.hpp"
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

template<typename type>
struct linked_node : type
{
    linked_node() : next()
    {
    }
    linked_node* next;
};

template<typename tt>
struct linked_list : fast_alloc<linked_node<tt>, 64 * 1024>
{
    typedef linked_node<tt> type;

private:
    type root;
    std::atomic<type*> tail;
    
    std::mutex& mutex;
    std::condition_variable& cond;

public:
    linked_list(std::mutex& mutex, std::condition_variable& cond) : tail(&root), mutex(mutex), cond(cond)
    {
    }
    void push(type* t) //push element in list, always success
    {
        bool flush = t->flush;
        type* expected = tail;
        while(!tail.compare_exchange_weak(expected, t)) {
            expected = tail;
        }
        expected->next = t;

        if(flush) {
            bool lock = mutex.try_lock();
            cond.notify_all();
            if(lock)
                mutex.unlock();
        }
    }
    type* next(type* prev) //can return nullptr, but next time caller should used latest not nullptr value
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
    context()
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

struct log_exporter
{
    log_exporter()
    {
    }
    static const char* name()
    {
        return "log_exporter";
    }
    void proceed(const message& m)
    {
        if(m.id == msg_book)
            mlog() << "<" << m.mb;
        else if(m.id == msg_trade)
            mlog() << "<" << m.mt;
        else if(m.id == msg_clean)
            mlog() << "<" << m.mc;
        else if(m.id == msg_instr)
            mlog() << "<" << m.mi;
        if(m.flush)
            mlog() << "<flush|";
    }
};

struct lib_exporter
{
    std::string module;
    exports exp;
    void proceed(const message& m)
    {
        exp.proceed(m);
    }
    const char* name() const
    {
        return module.c_str();
    }
    lib_exporter(const std::string& module) : module(module), exp(module)
    {
    }
    lib_exporter(const std::string& module, const std::string& params) : module(module), exp(module, params)
    {
    }
};

std::unique_ptr<lib_exporter> create_exporter_with_params(const std::string& m)
{
    auto ib = m.begin(), ie = m.end(), it = std::find(ib, ie, ' ');
    if(it == ie || it + 1 == ie)
        return std::make_unique<lib_exporter>(m);
    else
        return std::make_unique<lib_exporter>(std::string(ib, it), std::string(it + 1, ie));
}

class engine::impl : public stack_singleton<engine::impl>
{
    volatile bool can_run;
    std::mutex mutex;
    std::condition_variable cond;

    template<typename type>
    struct counter : type
    {
        std::atomic<uint32_t> cnt;
        counter()
        {
        }
    };

    typedef linked_list<counter<message> > llist;
    llist ll;


    std::vector<std::thread> threads; //consume workers, one thread for each exporter
    uint32_t consumers;
    template <typename consumer>
    void work_thread(std::unique_ptr<consumer> p)
    {
        try{
            llist::type *prev = nullptr, *ptmp;
            time_t ptime = 0;
            message mp = message();
            static_cast<msg_head&>(mp) = msg_head{message_ping::msg_id, message_ping::size};
            mp.flush = false;
            bool flush = true;
            while(can_run)
            {
                //lockfree when queue not empty
                ptmp = ll.next(prev);
        repeat:
                if(ptmp){
                    p->proceed(*ptmp);
                    flush = ptmp->flush;
                    if(flush)
                        ptime = time(NULL);
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
                else if(flush){
                    time_t ct = time(NULL);
                    if(ct != ptime) {
                        mp.mtime = get_cur_ttime();
                        p->proceed(mp);
                        ptime = ct;
                    }
                    else {
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
        }
        catch(std::exception& e){
            mlog(mlog::error) << str_holder(p->name()) << " " << e;
            //TODO: close connection for market data provider for current message
            //this should be done in server::impl::work_thread
        }
    }
    static void log_and_throw_error(const uint8_t* data, uint32_t size, const char* reason)
    {
        mlog() << "bad message (" << str_holder(reason) << "!): " << print_binary(data, std::min<uint32_t>(32, size));
        throw std::runtime_error("bad message");
    }
public:
    impl() : can_run(true), ll(mutex, cond)
    {
    }
    void init()
    {
        if(config::instance().log_exporter)
            threads.push_back(std::thread(&impl::work_thread<log_exporter>, this, std::make_unique<log_exporter>()));
        for(const auto& m: config::instance().exports)
            threads.push_back(std::thread(&impl::work_thread<lib_exporter>, this, create_exporter_with_params(m)));
        consumers = threads.size();
    }
    uint32_t proceed(const uint8_t* data, uint32_t size, context* ctx)
    {
        //MPROFILE("engine::proceed()")
        //mlog() << "proceed " << size << " bytes";
        uint32_t consumed = 0;

        while(size)
        {
            if(unlikely(size < sizeof(msg_head)))
                break;
            alloc_holder<llist> tm(ll);
            msg_head& mh = *tm;
            memcpy(&mh, data, sizeof(msg_head));
            const uint32_t msg_sz = mh.size + sizeof(msg_head);
#define MESSAGE_BEG(mes, member) \
            else if(mh.id == mes::msg_id) { \
                static const uint32_t cur_msg_size = mes::size; \
                if(unlikely(mh.size != cur_msg_size)) \
                    log_and_throw_error(data, size, es() % "cur_msg_size: " % cur_msg_size % ", mh.size: " % mh.size); \
                if(size < msg_sz) \
                    break; \
                mes* t = &(tm-> member); \
                memcpy(t, data + sizeof(msg_head), cur_msg_size);

#define MESSAGE_END() \
                consumed += msg_sz; \
                size -= msg_sz; \
                data += msg_sz; \
            }

#define MESSAGE_CONS_BEG(mes, member) \
            MESSAGE_BEG(mes, member) \
                tm->mtime = get_cur_ttime(); \
                if(unlikely(!t->time.value)) \
                    throw std::runtime_error("field time not set"); \
                tm->flush = !(size - msg_sz);

#define MESSAGE_CONS_END() \
                tm->cnt = consumers; \
                ll.push(tm.release()); \
            MESSAGE_END()

            if(0 == 1){
            }

            MESSAGE_CONS_BEG(message_book, mb)
                ctx->check(t->security_id, t->time);
                //mlog() << "|" << *t;
            MESSAGE_CONS_END()

            MESSAGE_CONS_BEG(message_trade, mt)
                ctx->check(t->security_id, t->time);
            MESSAGE_CONS_END()

            MESSAGE_CONS_BEG(message_clean, mc)
                ctx->check_clean(*t);
            MESSAGE_CONS_END()

            MESSAGE_CONS_BEG(message_instr, mi)
                uint32_t sec_id = get_security_id(*t, true);
                ctx->insert(sec_id, t->time);
            MESSAGE_CONS_END()

            MESSAGE_BEG(message_ping, mp)
                //mlog() << "<ping|" << t->time << "|";
            MESSAGE_END()

            MESSAGE_BEG(message_hello, dratuti)
                mlog() << "<hello|" << t->name << "|" << t->time << "|";
            MESSAGE_END()

            else {
                log_and_throw_error(data, size, es() % "bad msg_id: " % mh.id);
            }
        }
        if(consumed)
            cond.notify_all();
        return consumed;
    }
    ~impl()
    {
        can_run = false;
        for(auto&& t: threads)
            t.join();
    }
    void push_clean(uint32_t security_id, ttime_t parser_time, bool flush) //when parser disconnected all OrdersBooks cleans
    {
        auto p = ll.alloc();
        static_cast<msg_head&>(*p) = msg_head{message_clean::msg_id, message_clean::size};
        p->mtime = get_cur_ttime();
        p->mc = message_clean{security_id, 1/*source*/, parser_time};
        p->cnt = consumers;
        p->flush = flush;
        ll.push(p);
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
    auto it = data.begin(), ie = data.end();
    for(; it != ie; ++it) {
        engine::impl::instance().push_clean(it->security_id, it->time, it + 1 == ie);
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

uint32_t proceed_data(const uint8_t* data, uint32_t size, context* ctx)
{
    return engine::impl::instance().proceed(data, size, ctx);
}

