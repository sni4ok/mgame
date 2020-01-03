/*
    author: Ilya Andronov <sni4ok@yandex.ru>

    export = file file_type open_mode file_name

    file_type: bin, csv
    open_mode: truncate, append, rename_new
*/

#include "makoa/exports.hpp"
#include "tyra/tyra.hpp"

#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace {

struct efile
{
    std::vector<char> buf;
    buf_stream bs;
    bool bin, csv;
    std::string fname;
    int hfile;

    efile(const std::string& params) : buf(1024 * 1024), bs(buf), bin(), csv()
    {
        std::vector<std::string> p = split(params, ' ');
        if(p.size() != 3)
            throw std::runtime_error(es() % "efile() bad params: " % params);
        if(p[0] == "bin")
            bin = true;
        else if(p[0] == "csv")
            bin = csv;
        else
            throw std::runtime_error(es() % "efile() bad file_type: " % params);
        fname = std::move(p[2]);

        int fp = O_WRONLY | O_CREAT | O_APPEND;
        if(p[1] == "truncate")
            fp |= O_TRUNC;
        else if(p[1] == "append")
        {
        }
        else if(p[1] == "rename_new")
        {
            std::string backup = fname + "_" + std::to_string(time(NULL));
            int r = rename(fname.c_str(), backup.c_str());
            if(r) {
                mlog(mlog::critical) << "rename file from " << fname << ", to " << backup
                    << ", error: " << r << ", " << str_holder(errno ? strerror(errno) : "");
            }
            else {
                mlog(mlog::critical) << "file renamed from " << fname << ", to " << backup;
            }
            fp |= O_EXCL;
        }
        else
            throw std::runtime_error(es() % "efile() bad open_mode: " % params);

        hfile = ::open(fname.c_str(), fp, S_IWRITE | S_IREAD | S_IREAD | S_IWRITE | S_IRGRP | S_IWGRP);
        if(hfile < 0)
            throw_system_failure(es() % "open file " % fname % " error");
    }
    void write(const char* buf, uint32_t count)
    {
        if(::write(hfile, buf, count) != count)
            throw_system_failure(es() % "efile " % fname % " writing error");
    }
    void flush()
    {
        write(bs.begin(), bs.size());
        bs.clear();
    }
    void write_csv(const message_book& m)
    {
        bs << "b," <<  m.security_id << ',' << m.price << ',' << m.count << ',' << m.etime << ',' << m.time << '\n';
    }
    void write_csv(const message_trade& m)
    {
        bs << "t," <<  m.security_id << ',' << m.direction << ',' << m.price << ',' << m.count << ',' << m.etime << ',' << m.time << '\n';
    }
    void write_csv(const message_clean& m)
    {
        bs << "c," <<  m.security_id << ',' << m.source << ',' << m.time << '\n';
    }
    void write_csv(const message_instr& m)
    {
        bs << "i," <<  m.exchange_id << ',' << m.feed_id << ',' << m.security_id << ',' << m.time << '\n';
    }
    void proceed_bin(const message& m)
    {
        if(!m.flush) {
            bs.write((const char*)&m, sizeof(m));
        }
        else {
            if(bs.size()) {
                bs.write((const char*)&m, sizeof(m));
                flush();
            }
            else {
                write((const char*)&m, sizeof(m));
            }
        }
    }
    void proceed_csv(const message& m)
    {
        if(m.id == msg_book) {
            auto& v = reinterpret_cast<const tyra_msg<message_book>& >(m);
            write_csv(v);
        }
        else if(m.id == msg_trade) {
            auto& v = reinterpret_cast<const tyra_msg<message_trade>& >(m);
            write_csv(v);
        }
        else if(m.id == msg_clean) {
            auto& v = reinterpret_cast<const tyra_msg<message_clean>& >(m);
            write_csv(v);
        }
        else if(m.id == msg_instr) {
            auto& v = reinterpret_cast<const tyra_msg<message_instr>& >(m);
            write_csv(v);
        }
        else if(m.id == msg_ping) {
            flush();
        }
        if(m.flush) {
            flush();
        }
    }

    void proceed(const message& m)
    {
        if(bin)
            proceed_bin(m);
        else
            proceed_csv(m);
    }
    ~efile()
    {
        ::close(hfile);
    }
};

void* efile_init(const char* params)
{
    return new efile(params);
}

void efile_destroy(void* v)
{
    delete (efile*)(v);
}

void efile_proceed(void* v, const message& m)
{
    ((efile*)(v))->proceed(m);
}

}

extern "C"
{
    void create_hole(hole_exporter* m, simple_log* sl)
    {
        log_set(sl);
        m->init = &efile_init;
        m->destroy = &efile_destroy;
        m->proceed = &efile_proceed;
    }
}

