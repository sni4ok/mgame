/*
    This file contains definition of messages processed by makoa server with parsers
    
    ttime_t time;  //parser in time
    ttime_t etime; //exchange time
    uint8_t msg_id;

    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "messages_t.hpp"

static const uint8_t 
                          msg_ping  = 69
                        , msg_hello = 42
                        , msg_trade = 10
                        , msg_instr = 11
                        , msg_clean = 12
                        , msg_book  = 13
;

static const uint32_t message_size = 48, message_bsize = message_size - 17;

struct message_times
{
    ttime_t time;
    ttime_t etime;
};

struct message_id
{
    char _[16];
    uint8_t id;

    bool operator==(uint8_t i) const
    {
        return id == i;
    }
};

struct message_ping : message_times
{
    uint8_t id;
    uint8_t unused[message_bsize];

    static const uint32_t msg_id = msg_ping;
};
static_assert(sizeof(message_ping) == message_size, "protocol agreement");

struct message_hello : message_times
{
    uint8_t id;
    uint8_t unused[message_bsize - 16];

    char name[16];
    static const uint32_t msg_id = msg_hello;
};
static_assert(sizeof(message_hello) == message_size, "protocol agreement");

struct message_trade : message_times
{
    uint8_t id;
    uint8_t unused;

    //enum direction{unknown = 0, buy = 1, sell = 2};
    uint16_t direction;
    uint32_t security_id;
	int64_t unused_;
    price_t price;
    count_t count;
    static const uint32_t msg_id = msg_trade;
};
static_assert(sizeof(message_trade) == message_size, "protocol agreement");

//message_instr clean OrderBook for instrument
struct message_instr : message_times
{
    uint8_t id;
    char exchange_id[8];
    char feed_id[4];
    char security[15];
    
    uint32_t security_id; // = crc32(exchange_id, feed_id, security) now

    static const uint32_t msg_id = msg_instr;
};
static_assert(sizeof(message_instr) == message_size, "protocol agreement");

struct message_clean : message_times
{
    uint8_t id;
    uint8_t unused[message_bsize - 8];

    uint32_t security_id;
    uint32_t source; //0 from parsers, 1 from disconnect events
    static const uint32_t msg_id = msg_clean;
};
static_assert(sizeof(message_clean) == message_size, "protocol agreement");

struct message_book : message_times
{
    uint8_t id;
    uint8_t unused[3];

    uint32_t security_id;
	int64_t level_id;
    price_t price; 
    count_t count; //this new counts for level_id
    static const uint32_t msg_id = msg_book;
};
static_assert(sizeof(message_book) == message_size, "protocol agreement");

struct message
{
    union
    {
        message_id id;
        message_times t;
        message_book mb;
        message_trade mt;
        message_clean mc;
        message_instr mi;
        message_ping mp;
        message_hello dratuti;
    };
};

static_assert(sizeof(message) == message_size);

