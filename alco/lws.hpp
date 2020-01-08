/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "alco.hpp"

#include <libwebsockets.h>

struct lws_impl : emessages, stack_singleton<lws_impl>
{
    char buf[512];
    buf_stream bs;
    typedef const char* iterator;

    bool closed;
    time_t data_time;
    
    std::vector<std::string> subscribes;
    
    lws_impl() : emessages(config::instance().push), bs(buf, buf + sizeof(buf) - 1), closed(), data_time(time(NULL))
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
    ~lws_impl()
    {
        try {
            lws_context_destroy(context);
        } catch (std::exception& e) {
            mlog() << "~lws_impl() " << e;
        }
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
    mlog() << "callback: " << int(reason);
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
            ((lws_w*)user)->proceed(wsi, in, len);
            ((lws_w*)user)->data_time = time(NULL);
            return 0;
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        {
            mlog() << "lws closed";
            ((lws_w*)user)->closed = true;
            return -1;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            ((lws_w*)user)->closed = true;
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

template<typename lws_w>
void proceed_lws_parser(volatile bool& can_run)
{
    while(can_run) {
        try {
            lws_w ls;
            ls.context = create_context<lws_w>();
            connect(ls);

            int n = 0, i = 0;
            while(can_run && n >= 0 && !ls.closed) {
                if(!((++i) % 50)) {
                    i = 0;
                    if(ls.data_time + 10 < time(NULL))
                        throw std::runtime_error(es() % " no data from " % ls.data_time);
                }
                n = lws_service(ls.context, 0);
            }
        } catch(std::exception& e) {
            mlog() << "proceed_lws_parser " << e;
            if(can_run)
                usleep(5000 * 1000);
        }
        mlog(mlog::critical) << "proceed_lws_parser() recreate loop";
        if(can_run)
            usleep(1000 * 1000);
    }
}

