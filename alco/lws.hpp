/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "alco.hpp"

#include "evie/profiler.hpp"

#include <libwebsockets.h>

#include <sys/stat.h>
#include <fcntl.h>

struct lws_dump
{
    int hfile;
    bool lws_not_fake, lws_dump_en;
    lws_dump() : hfile(), lws_not_fake(true), lws_dump_en(), dump_buf("\n    \n")
    {
        char* dump = getenv("lws_dump");
        char* fake = getenv("lws_fake");
        if(dump && fake)
            throw std::runtime_error("lws_dump and lws_fake not works together");
        
        if(dump) {
            lws_dump_en = true;
            hfile = ::open(dump, O_WRONLY | O_CREAT | O_APPEND, S_IWRITE | S_IREAD | S_IRGRP | S_IWGRP);
            if(hfile < 0)
                throw_system_failure(es() % "lws_dump() open file " % _str_holder(dump) % " error");
            mlog() << "lws_dump to " << _str_holder(dump) << " enabled";
        }
        if(fake) {
            lws_not_fake = false;
            hfile = ::open(fake, O_RDONLY);
            if(hfile < 0)
                throw_system_failure(es() % "lws_fake() open file " % _str_holder(fake) % " error");
            dump_readed = 0;
            struct stat st;
            if(::fstat(hfile, &st))
                throw_system_failure("fstat() error");
            dump_size = st.st_size;
        }
    }
    char dump_buf[8];
    void dump(const char* p, uint32_t sz)
    {
        if(sz) {
            memcpy(dump_buf + 1, &sz, sizeof(sz));
            if(::write(hfile, dump_buf, 6) != 6)
                throw_system_failure("lws_dump writing error");
            if(::write(hfile, p, sz) != sz)
                throw_system_failure("lws_dump writing error");
        }
    }

    std::vector<char> read_buf;
    uint64_t dump_readed, dump_size;
    str_holder read_dump()
    {
        if(dump_readed < dump_size)
        {
            if(::read(hfile, dump_buf, 6) != 6)
                throw_system_failure("lws_dump reading error");
            uint32_t sz;
            memcpy(&sz, dump_buf + 1, sizeof(sz));
            read_buf.resize(sz);
            if(dump_buf[0] != '\n' || dump_buf[5] != '\n' || ::read(hfile, &read_buf[0], sz) != sz)
                throw_system_failure("lws_dump reading error");
            dump_readed += (sz + 6);
            return str_holder(&read_buf[0], sz);
        }
        return str_holder(nullptr, 0);
    }

    ~lws_dump()
    {
        if(hfile)
            ::close(hfile);
    }

};

struct lws_impl : emessages, lws_dump, stack_singleton<lws_impl>
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
        if(likely(lws_not_fake))
        for(auto& s: subscribes) {
            bs << s;
            send(wsi);
        }
    }
    void send(lws* wsi)
    {
        if(lws_not_fake && bs.size() != LWS_PRE) {
            int sz = bs.size() - LWS_PRE;
            char* ptr = bs.from + LWS_PRE;
            if(config::instance().log_lws)
                mlog() << "lws_write " << str_holder(ptr, sz);
            int n = lws_write(wsi, (unsigned char*)ptr, sz, LWS_WRITE_TEXT);
                
            if(unlikely(sz != n))
                throw std::runtime_error(es() % "lws_write ret: " % n % ", sz: " % sz);
        }
        bs.resize(LWS_PRE);
    }
    ~lws_impl()
    {
        try {
            if(lws_not_fake)
            lws_context_destroy(context);
        } catch (std::exception& e) {
            mlog() << "~lws_impl() " << e;
        }
    }

    lws_context* context;
};

template<typename str>
inline void skip_fixed(const char* &it, const str& v)
{
    static_assert(std::is_array<str>::value);
    bool eq = std::equal(it, it + sizeof(v) - 1, v);
    //bool eq = !(strncmp(it, v, sizeof(v) - 1));
    //bool eq = !(memcmp(it, v, sizeof(v) - 1));
    if(unlikely(!eq))
        throw std::runtime_error(es() % "skip_fixed error, expect: |" % str_holder(v) %  "| in |" % str_holder(it, 100));
    it += sizeof(v) - 1;
}

template<typename str>
inline void search_and_skip_fixed(const char* &it, const char* ie, const str& v)
{
    static_assert(std::is_array<str>::value);
    const char* i = std::search(it, ie, v, v + (sizeof(v) - 1));
    if(unlikely(i == ie))
        throw std::runtime_error(es() % "search_and_skip_fixed error, expect: |" % str_holder(v) %  "| in |" % str_holder(it, ie - it));
    it  = i + (sizeof(v) - 1);
}

template<typename str>
inline bool skip_if_fixed(const char* &it, const str& v)
{
    static_assert(std::is_array<str>::value);
    bool eq = std::equal(it, it + sizeof(v) - 1, v);
    //bool eq = !(strncmp(it, v, sizeof(v) - 1));
    //bool eq = !(memcmp(it, v, sizeof(v) - 1));
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
            lws_w* w = (lws_w*)user;
            if(unlikely(w->lws_dump_en))
                w->dump((const char*)in, len);
            if(unlikely(config::instance().log_lws))
                mlog() << "lws receive len: " << len;
            w->proceed(wsi, in, len);
            w->data_time = time(NULL);
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
            if(unlikely(config::instance().log_lws))
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
void proceed_lws_parser_fake(volatile bool& can_run)
{
    mlog() << "parser started in fake mode, from " << _str_holder(getenv("lws_fake"));
    lws_w ls;
    for(;can_run;)
    {
        str_holder str = ls.read_dump();
        if(str.size) {
            MPROFILE("lws_fake")
            ls.proceed(nullptr, (void*)str.str, str.size);
        }
        else
            break;
    }
}

template<typename lws_w>
void proceed_lws_parser(volatile bool& can_run)
{
    if(getenv("lws_fake"))
        proceed_lws_parser_fake<lws_w>(can_run);
    else
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

