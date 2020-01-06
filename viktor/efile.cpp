/*
    author: Ilya Andronov <sni4ok@yandex.ru>

    export = file file_type open_mode file_name

    file_type: bin, csv
    open_mode: truncate, append, rename_new
*/

#include "makoa/exports.hpp"
#include "makoa/types.hpp"
#include "evie/mlog.hpp"

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
        bs << "b," <<  m.security_id << ',' << m.price << ',' << m.count
            << ',' << m.etime << ',' << m.time << '\n';
    }
    void write_csv(const message_trade& m)
    {
        bs << "t," <<  m.security_id << ',' << m.direction << ',' << m.price << ',' << m.count
            << ',' << m.etime << ',' << m.time << '\n';
    }
    void write_csv(const message_clean& m)
    {
        bs << "c," <<  m.security_id << ',' << m.source
            << ',' << m.etime << ',' << m.time << '\n';
    }
    void write_csv(const message_instr& m)
    {
        bs << "i," <<  m.exchange_id << ',' << m.feed_id << ',' << m.security_id
            << ',' << m.time << '\n';
    }
    void proceed_csv(const message* m, uint32_t count)
    {
        for(uint32_t i = 0; i != count; ++i, ++m) {
            if(m->id == msg_book)
                write_csv((message_book&)*m);
            else if(m->id == msg_trade)
                write_csv((message_trade&)*m);
            else if(m->id == msg_clean)
                write_csv((message_clean&)*m);
            else if(m->id == msg_instr) {
                write_csv((message_instr&)*m);
            }
        }
        flush();
    }
    void proceed(const message* m, uint32_t count)
    {
        if(bin)
            write((const char*)m, message_size * count);
        else
            proceed_csv(m, count);
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

void efile_proceed(void* v, const message* m, uint32_t count)
{
    ((efile*)(v))->proceed(m, count);
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

