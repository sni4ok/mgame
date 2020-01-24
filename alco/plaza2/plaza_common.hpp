/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "config.hpp"

#include "evie/string.hpp"
#include "evie/mlog.hpp"

#include "makoa/messages.hpp"

#include "cgate.h"

#include <unistd.h>

#pragma pack(push, 4)
template<uint32_t sz>
struct cg_string
{
    char buf[sz + 1];
    cg_string(){
        memset(buf, 0, sizeof(buf));
    }
    template<typename array>
    cg_string(const array& v){
        static_assert(sizeof(v[0]) == 1);
        static_assert(std::is_array<array>::value);
        static_assert(sizeof(v) <= sizeof(buf));
        my_fast_copy(&v[0], sizeof(v), &buf[0]);
        memset(&buf[sizeof(v)], 0, sizeof(buf) - sizeof(v));
    }
    template<typename array>
    cg_string& operator=(const array& v){
        static_assert(sizeof(v[0]) == 1);
        static_assert(std::is_array<array>::value);
        static_assert(sizeof(v) <= sizeof(buf));
        my_fast_copy(&v[0], sizeof(v), &buf[0]);
        return *this;
    }
    cg_string& operator=(const std::string& str){
        if(str.size() > sz)
            throw std::runtime_error("cg_string::operator= size overflow");
        my_fast_copy(str.c_str(), str.size() + 1, &buf[0]);
        return *this;
    }
    void clear(){
        buf[0] = char();
    }
    bool empty() const{
        return buf[0] == char();
    }
};

struct cg_stream
{
    char* it;
    cg_stream(char* str) : it(str){
    }
    template<uint32_t sz>
    cg_stream(cg_string<sz>& str) : it(str.buf){
    }
    cg_stream& operator<<(char ch){
        *it = ch;
        ++it;
        return *this;
    }
    template<typename array>
    typename std::enable_if<std::is_array<array>::value, cg_stream&>::type operator<<(const array& v)
    {
        static_assert(std::is_array<array>::value);
        uint32_t sz = sizeof(v) / 1;
        if(v[sz - 1] == char())
            --sz;
        my_fast_copy(&v[0], sz, it);
        it += sz;
        return *this;
    }
    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, cg_stream&>::type operator<<(T t)
    {
        static_assert(std::is_integral<T>::value);
        uint32_t sz = my_cvt::itoa<T>(it, t);
        it += sz;
        return *this;
    }
    void write(const char* str, std::streamsize count)
    {
        my_fast_copy(str, count, it);
        it += count;
    }
};

template<uint32_t sz>
mlog& operator<<(mlog& ml, const cg_string<sz>& str)
{
    ml << str_holder(str.buf);
    return ml;
}


template<uint32_t m, uint32_t e>
struct cg_decimal
{
    char buf[2 + (m >> 1) + ((m | e) & 1)];
    cg_decimal(){
        memset(buf, 0, sizeof(buf));
    }
    price_t operator*() const{
        price_t ret;
        int8_t s;
        cg_bcd_get((void*)buf, &ret.value, &s);
        int8_t ds = s + price_t::exponent;
        if(!ds) {
        }
        else if(ds > 0) {
            ret.value /= my_cvt::decimal_pow[ds];
        }
        else {
            ret.value *= my_cvt::decimal_pow[-ds];
        }
        return ret;
    }
};

template<uint32_t m, uint32_t e>
mlog& operator<<(mlog& ml, const cg_decimal<m, e>& d)
{
    ml << *d;
    return ml;
}

inline mlog& operator<<(mlog& ml, const cg_time_t& t)
{
    mtime_parsed p;
    p.year = t.year;
    p.month = t.month;
    p.day = t.day;
    p.hours = t.hour;
    p.minutes = t.minute;
    p.seconds = t.second;
    p.nanos = t.msec * 1000 * 1000;
    ml << p;
    return ml;
}

#pragma pack(pop)

inline void check_plaza_fail(uint32_t res, const char* msg)
{
    if(unlikely(res != CG_ERR_OK)) {
        mlog(mlog::critical) << "Client gate error (" << _str_holder(msg) << "): " << res;
        throw std::runtime_error(es() % "Client gate error (" % msg % "): " % res);
    }
} 

inline void warn_plaza_fail(uint32_t res, const char* msg)
{
    if(unlikely(res != CG_ERR_OK)) {
        mlog(mlog::critical) << "Client gate warning (" << _str_holder(msg) << "): " << res;
    }
} 

struct cg_conn_h
{
    cg_conn_t *cli;
    std::string cli_conn;
    cg_conn_h(const std::string& cli_conn) : cli(), cli_conn(cli_conn)
    {
    }
    void wait_active(volatile bool& can_run)
    {
        for(;can_run;) {
            uint32_t state = 0;
            check_plaza_fail(cg_conn_getstate(cli, &state), "conn_getstate");
            if(state == CG_STATE_ERROR)
                throw std::runtime_error("Failed to open plaza2 connection");
            if(state == CG_STATE_ACTIVE)
                break;
            usleep(50);
        }
    }
    void connect(volatile bool& can_run)
    {
        mlog() << "cg_conn_new: " << cli_conn;
        check_plaza_fail(cg_conn_new(cli_conn.c_str(), &cli), "conn_new");
        check_plaza_fail(cg_conn_open(cli, 0), "conn_open");
        wait_active(can_run);
    }
    void disconnect()
    {
        if(cli) {
            warn_plaza_fail(cg_conn_close(cli), "conn_close");
            warn_plaza_fail(cg_conn_destroy(cli), "conn_destroy");
            cli = 0;
        }
    }
    ~cg_conn_h()
    {
        disconnect();
    }
};

typedef CG_RESULT (*plaza_func)(cg_conn_t* conn, cg_listener_t* listener, struct cg_msg_t* msg, void* data);
struct cg_listener_h
{
    cg_listener_t* listener;
    cg_conn_h& conn;
    bool closed;
    std::string name, cli_listener;
    plaza_func func;
    void* func_state;
    std::string def_state;
    std::string rev;
    time_t last_call_time;
    void set_call(){
        last_call_time = time(NULL);
    }
    cg_listener_h(cg_conn_h& conn, const std::string& name, const std::string cli_listener, plaza_func func, void* func_state = 0, std::string def_state = "")
        : listener(), conn(conn), closed(true), name(name), cli_listener(cli_listener), func(func), func_state(func_state), def_state(def_state)
    {
        set_call();
    }
    void set_replstate(cg_msg_t* msg)
    {
        if(def_state.empty())
            rev = "replstate=" + std::string((char*)msg->data);
        else
            rev = def_state + ";" + "replstate=" + std::string((char*)msg->data);
    }
    void set_closed()
    {
        closed = true;
    }
    void destroy()
    {
        if(listener){
            uint32_t res = cg_lsn_destroy(listener);
            listener = 0;
            if(res != CG_ERR_OK)
                mlog(mlog::critical) << "Client gate listener destroy " << name << " error: " << res;
            else
                mlog() << "listener " << name << " was destroyed";
            closed = true;
        }
    }
    const char* get_state() const
    {
        if(rev.empty()){
            if(def_state.empty())
                return 0;
            return def_state.c_str();
        }
        return rev.c_str();
    }
    void open(){
        destroy();
        mlog() << "open stream: " << name;
        check_plaza_fail(cg_lsn_new(conn.cli, cli_listener.c_str(), func, func_state, &listener), "lsn_new");
        mlog() << "lsn_new: " << cli_listener;
        check_plaza_fail(cg_lsn_open(listener, get_state()), "lsn_open");
        mlog() << "stream: " << name << " opened";
        mlog() << "state: " << _str_holder(get_state() ? get_state() : "");
        {
            uint32_t state = 0;
            uint32_t r = cg_lsn_getstate(listener, &state);
            if(r != CG_ERR_OK || (state != CG_STATE_ACTIVE && state != CG_STATE_OPENING)){
                mlog(mlog::critical) << "stream: " << name << ", bad state: " << state;
                throw std::runtime_error("open stream error");
            }
        }
        closed = false;
        set_call();
    }
    void reopen(){
        if(likely(!closed))
            return;
        open();
    }
    void reopen_unactive(){
        if(unlikely(!listener))
            open();
        else{
            time_t t = time(NULL);
            if(t > last_call_time + 5){
                uint32_t state = 0;
                uint32_t r = cg_lsn_getstate(listener, &state);
                if(r != CG_ERR_OK || state != CG_STATE_ACTIVE){
                    mlog(mlog::critical) << "stream: " << name << " unactive, cg_lsn_getstate returned: " << r << ", state: " << state;
                    open();
                }
            }
        }
    }
    ~cg_listener_h(){
        destroy();
    }
};

struct heartbeat_check
{
    int64_t min_delta_ms;
    uint32_t possible_delay_ms;
    heartbeat_check(uint32_t possible_delay_ms)
    : min_delta_ms(10000000), possible_delay_ms(possible_delay_ms){
    }
    bool proceed(const cg_time_t& server_time){
        mtime ct = get_cur_mtime();
        int64_t day_ms = ((ct.total_sec() - ct.day_seconds()) * 1000) + ct.ms();
        int64_t server_ms = server_time.hour * 3600 * 1000 + server_time.minute * 60 * 1000 + server_time.second * 1000 + server_time.msec;
        int64_t delta_ms = day_ms - server_ms;
        min_delta_ms++;
        //Forts somethimes change time by hand for 5 or 42 seconds at a time
        if(delta_ms > min_delta_ms + possible_delay_ms + 50){
            int64_t set_to = min_delta_ms + 50;
            mlog(mlog::critical) << "heartbeat_check force reset, min_delta_ms: " << min_delta_ms << ", delta_ms: " << delta_ms
                << ", set to: " << set_to;
            min_delta_ms = set_to;
        }
        if(delta_ms < min_delta_ms){
            mlog() << "heartbeat_check min_delta_ms set to: " << delta_ms;
            min_delta_ms = delta_ms;
        }
        return (delta_ms < min_delta_ms + possible_delay_ms);
    }
};

