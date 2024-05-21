/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "lws.hpp"

#include <libwebsockets.h>

#include <sys/stat.h>
#include <fcntl.h>

mstring join_tickers(const mvector<mstring>& tickers, bool quotes)
{
    my_stream s;
    for(uint32_t i = 0; i != tickers.size(); ++i) {
        if(i)
            s << ",";
        if(quotes)
            s << "\"";
        s << tickers[i];
        if(quotes)
            s << "\"";
    }
    return s.str();
}

lws_dump::lws_dump() : hfile(), lws_not_fake(true), lws_dump_en(), dump_buf("\n    \n")
{
    char* dump = getenv("lws_dump");
    char* fake = getenv("lws_fake");
    if(dump && fake)
        throw str_exception("lws_dump and lws_fake not works together");

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

void lws_dump::dump(char_cit p, uint32_t sz)
{
    if(sz) {
        memcpy(dump_buf + 1, &sz, sizeof(sz));
        if(::write(hfile, dump_buf, 6) != 6)
            throw_system_failure("lws_dump writing error");
        if(::write(hfile, p, sz) != sz)
            throw_system_failure("lws_dump writing error");
    }
}

str_holder lws_dump::read_dump()
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
    return str_holder(nullptr, nullptr);
}

lws_dump::~lws_dump()
{
    if(hfile)
        ::close(hfile);
}

lws_impl::lws_impl(const mstring& push, bool log_lws) : emessages(push), log_lws(log_lws), bs(buf, buf + sizeof(buf) - 1), closed(), data_time(time(NULL))
{
    bs.resize(LWS_PRE);
}

void lws_impl::init(lws* wsi)
{
    if(lws_not_fake)
    {
        for(auto& s: subscribes) {
            bs << s;
            send(wsi);
        }
    }
}

void lws_impl::send(lws* wsi)
{
    if(lws_not_fake && bs.size() != LWS_PRE) [[likely]]
    {
        int sz = bs.size() - LWS_PRE;
        char* ptr = bs.from + LWS_PRE;
        if(log_lws)
            mlog() << "lws_write " << str_holder(ptr, sz);
        int n = lws_write(wsi, (unsigned char*)ptr, sz, LWS_WRITE_TEXT);

        if(sz != n) [[unlikely]]
            throw mexception(es() % "lws_write ret: " % n % ", sz: " % sz);
    }
    bs.resize(LWS_PRE);
}

lws_impl::~lws_impl()
{
    try {
        if(lws_not_fake)
            lws_context_destroy(context);
    } catch (exception& e) {
        mlog() << "~lws_impl() " << e;
    }
}

int lws_event_cb(lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
    switch (reason)
    {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            mlog() << "lws connection established";
            ((lws_impl*)user)->init(wsi);
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE:
        {
            lws_impl* w = (lws_impl*)user;
            if(w->lws_dump_en) [[unlikely]]
                w->dump((char_cit)in, len);
            if(w->log_lws) [[unlikely]]
                mlog() << "lws receive len: " << len;
            w->proceed(wsi, (char_cit)in, len);
            w->data_time = time(NULL);
            return 0;
        }
        case LWS_CALLBACK_CLIENT_CLOSED:
        {
            mlog() << "lws closed";
            ((lws_impl*)user)->closed = true;
            return -1;
        }
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        {
            ((lws_impl*)user)->closed = true;
            mlog() << "lws closed... " << (len ? str_holder((char_cit)in, len) : str_holder(""));
            return 1;
        }
        default:
        {
            if(user && ((lws_impl*)user)->log_lws) [[unlikely]]
                mlog() << "lws callback: " << int(reason) << ", len: " << len;
            break;
        }
    }
    return 0;
}

lws_context* create_context(char_cit ssl_ca_file)
{
    int logs = LLL_ERR | LLL_WARN ;
    lws_set_log_level(logs, NULL);
    lws_context_creation_info info;
    memset(&info, 0, sizeof (info));
    info.port = CONTEXT_PORT_NO_LISTEN;

    lws_protocols protocols[] =
    {
        {"ws", &lws_event_cb, 0, 4096 * 1000, 0, 0, 0},
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
        throw str_exception("lws_create_context error");
    return context;
}

int lws_impl::service()
{
    return lws_service(context, 0);
}

void lws_connect(lws_impl& ls, char_cit host, uint32_t port, char_cit path)
{
    lws_client_connect_info ccinfo = lws_client_connect_info();
    ccinfo.context = ls.context;
    ccinfo.address = host;
    ccinfo.port = port;
    ccinfo.path = path;
    ccinfo.userdata = (void*)&ls;
    ccinfo.host = ccinfo.address;
    ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    lws_client_connect_via_info(&ccinfo);
}

