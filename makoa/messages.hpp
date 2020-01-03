/*
    This file contains definition of messages processed by makoa server with parsers
    every message should start with msg_head: 
    uint32_t msg_id;
    uint32_t msg_size;
    ---- message ----
    -----------------

    all messages contains field:
        ttime_t time;  //parser time

    for ping, hello it should contains current parser time, so when its differs from makoa server time on 3h or -2 sec
        server will close connection

    for instr, trade, clean, book messages (they contains security_id field) this field can contains any time
        from 'have data for parsing' till 'send data to makoa' by parser decision
        this messages should be monotonically nondecreasing by time for certain security_id
    
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include <cstdint>

struct msg_head
{
    uint32_t id, size;
};
static_assert(sizeof(msg_head) == 8, "protocol agreement");

static const uint32_t 
                          msg_ping  = 69
                        , msg_hello = 42
                        , msg_trade = 10
                        , msg_instr = 11
                        , msg_clean = 12
                        , msg_book  = 13
;

#ifndef TTIME_T_DEFINED
struct ttime_t
{
    //value equals (unix_time * 10^9 + nanoseconds)
    static const uint32_t frac = 1000000000;
    uint64_t value;
};
#define TTIME_T_DEFINED
#endif

struct message_ping
{
    ttime_t time;

    static const uint32_t msg_id = msg_ping, size = 8;
};
static_assert(sizeof(message_ping) == 8, "protocol agreement");

struct message_hello
{
    char name[16];
    uint64_t reserved;
    ttime_t time;

    static const uint32_t msg_id = msg_hello, size = 32;
};
static_assert(sizeof(message_hello) == 32, "protocol agreement");

struct price_t
{
    static const int32_t exponent = -5;
    static const uint32_t frac = 100000;
    int64_t value;
};

struct count_t
{
    static const int32_t exponent = -8;
    static const uint32_t frac = 100000000;
    int64_t value;
};

struct message_trade
{
    uint32_t security_id;
    //enum direction{unknown = 0, buy = 1, sell = 2};
    uint32_t direction;
    price_t price;
    count_t count;
    ttime_t etime; //exchange time
    ttime_t time;  //parser time
    static const uint32_t msg_id = msg_trade, size = 40;
};
static_assert(sizeof(message_trade) == 40, "protocol agreement");

//message_instr clean OrderBook for instrument
struct message_instr
{
    char exchange_id[8];
    char feed_id[4];
    char security[16];
    
    uint32_t security_id; // = crc32(exchange_id, feed_id, security) now
    ttime_t time;  //parser time

    static const uint32_t msg_id = msg_instr, size = 40;
};
static_assert(sizeof(message_instr) == 40, "protocol agreement");

struct message_clean
{
    uint32_t security_id;
    uint32_t source; //0 from parsers, 1 from disconnect events
    ttime_t time;
    static const uint32_t msg_id = msg_clean, size = 16;
};
static_assert(sizeof(message_clean) == 16, "protocol agreement");

struct message_book
{
    uint32_t security_id;
    uint32_t reserved;
    price_t price; 
    count_t count; //this new counts for level price
    ttime_t etime; //exchange time
    ttime_t time;  //parser time
    static const uint32_t msg_id = msg_book, size = 40;
};
static_assert(sizeof(message_book) == 40, "protocol agreement");

struct message_bytes : msg_head
{
    union
    {
        message_book mb;
        message_trade mt;
        message_clean mc;
        message_instr mi;

        //ping and hello not proceed to export from makoa_server
        //message_ping using for flush
        message_ping mp;
        message_hello dratuti;
    };
};

struct message : message_bytes
{
    ttime_t mtime; //makoa server time
    bool flush; //when it set, all data from stream is consumed
};

static_assert(sizeof(message) == 64);

