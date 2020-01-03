/*
    author: Ilya Andronov <sni4ok@yandex.ru>

    export = mysql open_mode host port dbname user password
	open_mode: truncate, append, rename_new

    mysql create or recreate tables: instruments, orders, trades
*/

#include "makoa/exports.hpp"
#include "tyra/tyra.hpp"

#include <string>

#include <mysql/mysql.h>

namespace {

struct mysql
{
    std::vector<char> bufi, bufo, buft;
    buf_stream bsi, bso, bst;
    uint32_t si, so, st;

    std::unique_ptr<MYSQL, decltype(&mysql_close)> sql;
    bool truncate, rename_new;
    static const uint32_t prealloc = 1024 * 1024;
    mysql(const std::string& params) : bufi(prealloc), bufo(prealloc), buft(prealloc),
       bsi(bufi), bso(bufo), bst(buft), sql(mysql_init(0), &mysql_close),
        truncate(), rename_new()
    {
        if(!sql)
            throw std::runtime_error("mysql() mysql_init error");
        std::vector<std::string> p = split(params, ' ');
        if(p.size() != 6)
            throw std::runtime_error(es() % "mysql() bad params: " % params);

        if(p[0] == "truncate")
            truncate = true;
        else if(p[1] == "append")
        {
        }
        else if(p[0] == "rename_new")
            rename_new = true;
        else
            throw std::runtime_error(es() % "mysql() bad open_mode: " % params);

        if(!mysql_real_connect(sql.get(), p[1].c_str(), p[4].c_str(), p[5].c_str(), p[3].c_str(), lexical_cast<uint16_t>(p[2]), NULL, 0))
            throw_error();

        init_tables();

        bsi << "replace into instruments values";
        si = bsi.size();
        bso << "insert into orders values";
        so = bso.size();
        bst << "insert into trades values";
        st = bst.size();
    }
    void init_tables()
    {
        if(truncate)
        {
            if(mysql_query(sql.get(), "drop table if exists instruments"))
                throw_error();
            if(mysql_query(sql.get(), "drop table if exists orders"))
                throw_error();
            if(mysql_query(sql.get(), "drop table if exists trades"))
                throw_error();
        }
        if(rename_new)
        {
            time_t tt = time(NULL);
            mlog() << "rename tables instruments, orders, trades to "
                << "instruments_" << tt << ", orders_" << tt << ", trades_" << tt;
            bsi << "rename table instruments to instruments_" << tt;
            if(mysql_real_query(sql.get(), bsi.begin(), bsi.size()))
                mlog(mlog::critical) << "mysql error: " << str_holder(mysql_error(sql.get()));
            bsi.clear();
            bsi << "rename table orders to orders_" << tt;
            if(mysql_real_query(sql.get(), bsi.begin(), bsi.size()))
                mlog(mlog::critical) << "mysql error: " << str_holder(mysql_error(sql.get()));
            bsi.clear();
            bsi << "rename table trades to trades_" << tt;
            if(mysql_real_query(sql.get(), bsi.begin(), bsi.size()))
                mlog(mlog::critical) << "mysql error: " << str_holder(mysql_error(sql.get()));
            bsi.clear();
        }
 
        bsi << "create table if not exists instruments ( \
            exchange_id char(" << sizeof(message_instr::exchange_id) << ") not null, \
            feed_id char(" << sizeof(message_instr::feed_id) << ") not null, \
            security char(" << sizeof(message_instr::security) << ") not null, \
            security_id int unsigned not null, \
            time bigint unsigned not null, \
            primary key(security_id) \
        ) engine myisam";

        if(mysql_real_query(sql.get(), bsi.begin(), bsi.size()))
            throw_error();
        bsi.clear();

        //null price in orders mean clear msg_clean
        bsi << "create table if not exists orders ( \
            security_id int unsigned not null, \
            price decimal(19, 5) null, \
            count decimal(19, 8) not null, \
            etime bigint unsigned not null, \
            time bigint unsigned not null, \
            key(time) \
        ) engine myisam";
        
        if(mysql_real_query(sql.get(), bsi.begin(), bsi.size()))
            throw_error();
        bsi.clear();
        
        bsi << "create table if not exists trades ( \
            security_id int unsigned not null, \
            direction int unsigned not null, \
            price decimal(19, 5) not null, \
            count decimal(19, 8) not null, \
            etime bigint unsigned not null, \
            time bigint unsigned not null, \
            key(time) \
        ) engine myisam";
        
        if(mysql_real_query(sql.get(), bsi.begin(), bsi.size()))
            throw_error();
        bsi.clear();
    }
    void throw_error()
    {
        throw std::runtime_error(mysql_error(sql.get()));
    }
    void flush_stream(buf_stream& bs, uint32_t s)
    {
        std::string tmp(bs.begin(), bs.begin() + bs.size());
        if(bs.size() != s) {
            if(mysql_real_query(sql.get(), bs.begin(), bs.size()))
                throw_error();
            bs.resize(s);
        }
    }
    void flush()
    {
        flush_stream(bsi, si);
        flush_stream(bso, so);
        flush_stream(bst, st);
    }
    void clean(uint32_t security_id, ttime_t time)
    {
        if(bso.size() != so)
            bso << ',';
        bso << '(' << security_id << ",NULL,0,0," << time << ')';
    }
    void proceed(const message& m)
    {
        if(m.id == msg_book) {
            auto& v = reinterpret_cast<const tyra_msg<message_book>& >(m);
            if(bso.size() != so)
                bso << ',';
            bso << '(' << v.security_id << ',' << v.price << ',' << v.count << ',' << v.etime << ',' << v.time << ')';
        }
        else if(m.id == msg_trade) {
            auto& v = reinterpret_cast<const tyra_msg<message_trade>& >(m);
            if(bst.size() != st)
                bst << ',';
            bst << '(' << v.security_id << ',' << v.direction << ',' << v.price << ',' << v.count << ',' << v.etime << ',' << v.time << ')';
        }
        else if(m.id == msg_clean) {
            auto& v = reinterpret_cast<const tyra_msg<message_clean>& >(m);
            clean(v.security_id, v.time);
        }
        else if(m.id == msg_instr) {
            auto& v = reinterpret_cast<const tyra_msg<message_instr>& >(m);
            if(bsi.size() != si)
                bsi << ',';
            bsi << "(\'" << v.exchange_id << "\',\'" << v.feed_id << "\',\'" << v.security << "\'," << v.security_id << ',' << v.time << ')';
            clean(v.security_id, v.time);
        }
        else if(m.id == msg_ping) {
            flush();
        }
        if(m.flush) {
            flush();
        }
    }
};

void* mysql_init(const char* params)
{
    return new mysql(params);
}

void mysql_destroy(void* v)
{
    delete (mysql*)(v);
}

void mysql_proceed(void* v, const message& m)
{
    ((mysql*)(v))->proceed(m);
}

}

extern "C"
{
    void create_hole(hole_exporter* m, simple_log* sl)
    {
        log_set(sl);
        m->init = &mysql_init;
        m->destroy = &mysql_destroy;
        m->proceed = &mysql_proceed;
    }
}

