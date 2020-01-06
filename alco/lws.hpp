#include "makoa/exports.hpp"

#include <libwebsockets.h>

struct lws_impl : stack_singleton<lws_impl>
{
    exporter e;
    char buf[512];
    buf_stream bs;
    typedef const char* iterator;

    static const uint32_t pre_alloc = 150;
    message ms[pre_alloc];
    uint32_t m_s;
    
    std::vector<std::string> subscribes;
    
    lws_impl() : e(config::instance().push), bs(buf, buf + sizeof(buf) - 1), m_s()
    {
        bs.resize(LWS_PRE);
    }
    void init(lws* wsi)
    {
        for(auto& s: subscribes) {
            bs << s;
            send(wsi);
        }
    }
    void send(lws* wsi)
    {
        if(bs.size() != LWS_PRE) {
            int sz = bs.size() - LWS_PRE;
            char* ptr = bs.from + LWS_PRE;
            if(config::instance().log_lws)
                mlog() << "lws_write " << str_holder(ptr, sz);
            int n = lws_write(wsi, (unsigned char*)ptr, sz, LWS_WRITE_TEXT);
                
            if(unlikely(sz != n))
                throw std::runtime_error(es() % "lws_write ret: " % n % ", sz: " % sz);
            bs.resize(LWS_PRE);
        }
    }
    void add_instrument(const message_instr& mi)
    {
        if(unlikely(m_s == pre_alloc))
            send_messages();
        ms[m_s++].mi = mi;
    }
    void add_clean(uint32_t security_id, ttime_t etime, ttime_t time)
    {
        if(unlikely(m_s == pre_alloc))
            send_messages();
        
        message_clean& c = ms[m_s++].mc;
        c.time = time;
        c.etime = etime;
        c.id = msg_clean;
        c.security_id = security_id;
        c.source = 0;
    }
    void add_order(uint32_t security_id, price_t price, count_t count, ttime_t etime, ttime_t time)
    {
        if(unlikely(m_s == pre_alloc))
            send_messages();

        message_book& m = ms[m_s++].mb;
        m.time = time;
        m.etime = etime;
        m.id = msg_book;
        m.security_id = security_id;
        m.price = price;
        m.count = count;
    }
    void add_trade(uint32_t security_id, price_t price, count_t count, uint32_t direction, ttime_t etime, ttime_t time)
    {
        if(unlikely(m_s == pre_alloc))
            send_messages();

        message_trade& m = ms[m_s++].mt;
        m.time = time;
        m.etime = etime;
        m.id = msg_trade;
        m.direction = direction;
        m.security_id = security_id;
        m.price = price;
        m.count = count;
    }
    void send_messages()
    {
        if(m_s) {
            e.proceed(ms, m_s);
            m_s = 0;
        }
    }
    ~lws_impl()
    {
        lws_context_destroy(context);
    }

    lws_context* context;
};

template<typename str>
inline void skip_fixed(const char*  &it, const str& v)
{
    static_assert(std::is_array<str>::value);
    bool eq = std::equal(it, it + sizeof(v) - 1, v);
    if(unlikely(!eq))
        throw std::runtime_error(es() % "bad message: " % str_holder(it, 100));
    it += sizeof(v) - 1;
}

template<typename str>
inline bool skip_if_fixed(const char*  &it, const str& v)
{
    static_assert(std::is_array<str>::value);
    bool eq = std::equal(it, it + sizeof(v) - 1, v);
    if(eq)
        it += sizeof(v) - 1;
    return eq;
}

template<typename lws_w>
int lws_event_cb(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            mlog() << "lws connection established";
            ((lws_w*)user)->init(wsi);
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            if(config::instance().log_lws)
                mlog() << "lws receive len: " << len;
            return ((lws_w*)user)->proceed(wsi, in, len);
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        {
            mlog() << "lws closed";
            return -1;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            mlog() << "lws closed...";
            return 1;
        }
        default:
        {
            if(config::instance().log_lws)
                mlog() << "lws callback: " << int(reason) << ", len: " << len;
            break;
        }
    }
    return 0;
}

template<typename lws_w>
lws_context* create_context(const char* ssl_ca_file = 0)
{
    int logs = LLL_ERR | LLL_WARN ;
    lws_set_log_level(logs, NULL);
    lws_context_creation_info info;
    memset(&info, 0, sizeof (info));
    info.port = CONTEXT_PORT_NO_LISTEN;

    lws_protocols protocols[] =
    {
        {"example-protocol", &lws_event_cb<lws_w>, 0, 4096 * 100, 0, 0, 0},
        lws_protocols()
    };

    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    if(ssl_ca_file)
        info.client_ssl_ca_filepath = ssl_ca_file;
    lws_context* context = lws_create_context(&info);
    if(!context)
        throw std::runtime_error("lws_create_context error");
    return context;
}

