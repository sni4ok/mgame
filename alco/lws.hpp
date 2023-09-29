/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "alco.hpp"

#include "evie/profiler.hpp"
#include "evie/mlog.hpp"

#include <unistd.h>

struct lws_dump
{
    int hfile;
    bool lws_not_fake, lws_dump_en;
    char dump_buf[8];
    mvector<char> read_buf;
    uint64_t dump_readed, dump_size;

    lws_dump();
    void dump(char_cit p, uint32_t sz);
    str_holder read_dump();
    ~lws_dump();
};

struct lws_context;
struct lws;

struct lws_impl : emessages, lws_dump, stack_singleton<lws_impl>
{
    typedef char_cit iterator;

    const bool log_lws;
    char buf[512];
    buf_stream bs;
    bool closed;
    time_t data_time;
    mvector<mstring> subscribes;
    lws_context* context;
    
    lws_impl(const mstring& push, bool log_lws);
    void init(lws* wsi);
    void send(lws* wsi);
    ~lws_impl();

    int service();
    virtual void proceed(lws* wsi, char_cit in, size_t len) = 0;
};

lws_context* create_context(char_cit ssl_ca_file = 0);

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
            ls.proceed(nullptr, str.str, str.size);
        }
        else
            break;
    }
}

template<typename lws_w>
void proceed_lws_parser(volatile bool& can_run)
{
    set_can_run(&can_run);
    if(getenv("lws_fake"))
        proceed_lws_parser_fake<lws_w>(can_run);
    else
    while(can_run)
    {
        try
        {
            lws_w ls;
            ls.context = create_context();
            connect(ls);

            int n = 0, i = 0;
            while(can_run && n >= 0 && !ls.closed)
            {
                if(!((++i) % 50))
                {
                    i = 0;
                    if(ls.data_time + 10 < time(NULL))
                        throw mexception(es() % " no data from " % ls.data_time);
                }
                n = ls.service();
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

void lws_connect(lws_impl& ls, char_cit host, uint32_t port, char_cit path);

