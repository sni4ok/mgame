/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "makoa/types.hpp"

#include "evie/fmap.hpp"
#include "evie/mfile.hpp"
#include "evie/utils.hpp"
#include "evie/profiler.hpp"

#include <map>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static ttime_t parse_time(const std::string& time)
{
    const char* it = time.c_str();
    return read_time_impl().read_time<0>(it);
}

std::string::const_iterator find_last(const std::string& fname, const char c)
{
    std::string::const_iterator it = fname.end() - 1, ib = fname.begin();
    while(it != ib && *it != c)
        --it;
    return it;
}

struct compact_book
{
    std::vector<char> book;
    uint64_t book_off;

    compact_book() : book_off()
    {
    }

    void read(const std::string& fname, uint64_t from, uint64_t to)
    {
        if((to - from) % message_size)
            throw std::runtime_error(es() % "compact_book::read() " % fname % " from " % from % " to " % to);

        struct orders_t : std::map<int64_t/*level_id*/, message_book>
        {
            message_instr mi;

            orders_t() : mi()
            {
            }
        };
        ttime_t last_time = ttime_t();
        std::map<uint32_t/*security_id*/, orders_t> orders;

        //read
        std::vector<message> buf((to - from) / message_size);
        mfile f(fname.c_str());
        f.seekg(from);
        f.read((char*)&buf[0], to - from);
        auto it = buf.begin(), ie = buf.end();

        for(; it != ie; ++it) {
            if(it->id == msg_book) {
                const message_book& m = it->mb;
                orders_t& o = orders[m.security_id];
                if(o.mi.security_id) {
                    message_book& mb = o[m.level_id];
                    price_t price = m.price.value ? m.price : mb.price;
                    mb = m;
                    mb.price = price;
                }
            }
            else if(it->id == msg_instr) {
                orders_t& o = orders[it->mi.security_id];
                o.clear();
                o.mi = it->mi;
            }
            else if(it->id == msg_clean)
                orders.erase(it->mc.security_id);
            else {
                assert(it->id == msg_trade || it->id == msg_clean || it->id == msg_ping);
            }
            last_time = it->mt.time;
        }

        //compact
        buf.clear();
        auto i = orders.begin(), e = orders.end();

        for(; i != e; ++i) {
            assert(i->second.mi.id == msg_instr);
            message m;
            m.mi = i->second.mi;
            m.mi.time = last_time;
            m.mi.etime = ttime_t();
            buf.push_back(m);
        }
        i = orders.begin();

        for(; i != e; ++i) {
            auto it = i->second.begin(), ie = i->second.end();
            for(; it != ie; ++it) {
                if(it->second.count.value) {
                    message m;
                    assert(it->second.price.value);
                    m.mb = it->second;
                    m.mi.time = last_time;
                    m.mi.etime = ttime_t();
                    buf.push_back(m);
                }
            }
        }

        book.resize(buf.size() * message_size);
        std::copy(&buf[0], &buf[0] + buf.size(), (message*)&book[0]);
        mlog() << "compact_book::read() " << fname << " from " << from << " to " << to;
    }
};

struct ifile
{
    struct node
    {
        std::string name;
        ttime_t tf, tt;
        uint64_t from, off, sz;

        bool operator<(const node& n) const
        {
            return tf > n.tf;
        }
        bool crossed(const node& n) const
        {
            return (tt > n.tf && n.tt > tf);
        }
    };

    int cur_file;
    compact_book cb;
    const node main_file;
    std::vector<node> files;
    node nt;
    message mt;
    bool history;
    message_ping ping;
    std::vector<message> read_buf;
    static const int64_t check_time = {1 * ttime_t::frac};
    ttime_t last_file_check_time;
    ino_t last_file_ino;

    template<class type, class pred>
    static int64_t lower_bound_int(int64_t first, int64_t last, const type& val, pred pr)
    {
        int64_t count = last - first;
        while(count > 0) {
            int64_t step = count / 2;
            int64_t mid = first + step;
            if(pr(mid, val))
                first = ++mid, count -= step + 1;
            else
                count = step;
        }
        return first;
    }

    static ino_t file_ino(int handle, uint64_t* fsize = nullptr)
    {
        struct stat st;
        if(fstat(handle, &st))
            throw_system_failure(es() % "fstat() error for " % handle);
        if(fsize)
            *fsize = st.st_size;
        return st.st_ino;
    }

    ttime_t add_file_impl(const std::string& fname, bool last_file)
    {
        mfile f(fname.c_str());
        if(last_file)
            last_file_ino = file_ino(f.hfile);
        uint64_t sz = f.size();
        if(sz < message_size)
            return ttime_t();
        sz = sz - sz % message_size;
        f.read((char*)&mt, message_size);
        nt.tf = mt.t.time;
        if(!nt.tf.value)
            throw std::runtime_error(es() % "time_from error for " % fname);
        nt.from = 0;
        nt.off = 0;
        nt.sz = last_file ? 0 : sz;
        f.seekg(sz - message_size);
        f.read((char*)&mt, message_size);
        nt.tt = mt.t.time;
        if(!nt.tt.value || nt.tt.value < nt.tf.value)
            throw std::runtime_error(es() % "time_to error for " % fname);

        if(main_file.crossed(nt) || (!history && last_file)) {
            nt.name = fname;
            auto pred =
                [&f](int64_t off, ttime_t tt) {
                    message m;
                    f.seekg(off * message_size);
                    f.read((char*)&m, message_size);
                    return m.mt.time < tt;
                };

            if(nt.tt > main_file.tf || last_file) {
                assert(nt.from == 0 && nt.off == 0);
                static const uint64_t buf_size = message_size * 1024 * 1024;
                read_buf.resize(buf_size / message_size);
                nt.off = message_size * lower_bound_int(0, sz / message_size, main_file.tf, pred);
                fmap<uint32_t/*security_id*/, uint64_t /*off*/> tickers;
                uint64_t off = nt.off;

                while(!nt.from && off) {
                    nt.from = off < buf_size ? 0 : off - buf_size;
                    f.seekg(nt.from);
                    f.read((char*)&read_buf[0], off - nt.from);
                    auto it = read_buf.begin(), ie = read_buf.begin() + ((off - nt.from) / message_size);
                    off = nt.from;

                    for(; it != ie; ++it, nt.from += message_size)
                    {
                        if(it->id == msg_instr)
                            tickers[it->mi.security_id] = nt.from;
                        else if(it->id == msg_book)
                            tickers[it->mb.security_id];
                        else if(it->id == msg_trade)
                            tickers[it->mt.security_id];
                    }

                    for(auto& v: tickers)
                        nt.from = std::min(nt.from, v.second);
                }
            }

            if((nt.tt > main_file.tt) || (last_file && history))
                nt.sz = message_size * lower_bound_int(nt.off / message_size, sz / message_size, main_file.tt, pred);

            mlog() << "ifile::add_file_impl() " << fname << ", last: " << last_file << ", from: "
                << nt.from << ", off: " << nt.off << ", sz: " << nt.sz;

            files.push_back(nt);
            return nt.tt;
        }
        return ttime_t();
    }

    ttime_t add_file(const std::string& fname, bool last_file = false)
    {
        try {
            return add_file_impl(fname, last_file);
        }
        catch(std::exception& e) {
            mlog() << "ifile::add_file(" << fname << ") skipped, " << e;
        }
        return ttime_t();
    }

    void read_directory(const std::string& fname)
    {
        MPROFILE("ifile::read_directory")
        std::string::const_iterator it = find_last(fname, '/') + 1;
        std::string dir(fname.begin(), it);
        std::string f(it, fname.end());

        dirent **ee;
        int n = scandir(dir.c_str(), &ee, NULL, alphasort);
        if(n == -1)
            throw_system_failure(es() % "scandir error " % dir);

        uint32_t f_size = f.size();
        uint32_t tfrom = main_file.tf.value / ttime_t::frac;
        for(int i = 0; i != n; ++i) {
            dirent *e = ee[i];
            str_holder fname(_str_holder(e->d_name));
            if(fname.size > f_size && std::equal(fname.str, fname.str + f_size, f.begin())) {
                if(fname.size > f_size) {
                    if(fname.str[f_size] != '_')
                        continue;
                    uint32_t t = my_cvt::atoi<uint32_t>(&fname.str[f_size + 1], fname.size - f_size - 1);
                    if(t < tfrom)
                        continue;
                }
                ttime_t t = add_file(dir + e->d_name);
                if(t > main_file.tt)
                    break;
            }
            free(e);
        }
        add_file(fname, true);
        free(ee);
        std::sort(files.begin(), files.end());
        if(!files.empty()) {
            auto it = files.rbegin();
            cb.read(it->name, it->from, it->off);
        }
    }

    ifile(const std::string& fname, ttime_t tf, ttime_t tt, bool history)
        : cur_file(), main_file({fname, tf, tt, 0, 0, 0}), history(history), last_file_check_time()
    {
        read_directory(fname);
        ping.id = msg_ping;
    }

    bool last_file_moved()
    {
        int f = ::open(main_file.name.c_str(), O_RDONLY);
        if(f < 0)
            return false;
        uint64_t sz = 0;
        ino_t c = file_ino(f, &sz);
        if(c != last_file_ino && sz >= message_size)
        {
            mlog() << main_file.name << " probably moved";
            ::close(cur_file);
            last_file_ino = c;
            cur_file = f;
            nt.off = 0;
            return true;
        }
        ::close(f);
        return false;
    }

    virtual uint32_t read(char* buf, uint32_t buf_size)
    {
        if(unlikely(buf_size % message_size))
            throw std::runtime_error(es() % "ifile::read(): inapropriate size: " % buf_size);
        else {
            int32_t ret = 0;
            while(!ret)
            {
                if(!cur_file) {
                    if(!cb.book.empty()) {
                        uint32_t read_size = std::min<uint32_t>(buf_size, cb.book.size() - cb.book_off);
                        std::copy(&cb.book[0] + cb.book_off, &cb.book[0] + cb.book_off + read_size, buf);
                        cb.book_off += read_size;
                        if(cb.book_off == cb.book.size())
                            cb.book.clear();
                        return read_size;
                    }
                    if(files.empty())
                        return 0;
                    
                    nt = *files.rbegin();
                    files.pop_back();
                    cur_file = ::open(nt.name.c_str(), O_RDONLY);
                    if(::lseek(cur_file, nt.off, SEEK_SET) < 0)
                        throw_system_failure(es() % "lseek() error for " % nt.name);
                }

                if(!history && !nt.sz && files.empty())
                    ret = ::read(cur_file, buf, buf_size);
                else
                    ret = ::read(cur_file, buf, std::min<uint64_t>(buf_size, nt.sz - nt.off));

                if(ret > 0)
                {
                    if(ret % message_size)
                    {
                        MPROFILE("ifile::fix_read_size()")
                        int32_t d = (ret % message_size);
                        ret = ret - d;
                        if(::lseek(cur_file, -d, SEEK_CUR) < 0)
                            throw_system_failure(es() % "lseek() error for " % nt.name);
                    }
                    nt.off += ret;
                }
                if(ret < 0)
                    throw_system_failure("ifile::read file error");
                else if(!ret)
                {
                    if(!files.empty()) {
                        close(cur_file);
                        cur_file = 0;
                    }
                    else if(history)
                        return ret;
                    else {
                        ping.time = cur_ttime();
                        if(ping.time + check_time > last_file_check_time)
                        {
                            last_file_check_time = ping.time;
                            if(last_file_moved())
                                continue;
                        }
                        usleep(10 * 1000);
                        if(ping.time > main_file.tt)
                            return ret;
                        memcpy(buf, &ping, message_size);
                        return message_size;
                    }
                }
            }
            return ret;
        }
    }

    virtual ~ifile()
    {
        if(cur_file > 0)
            close(cur_file);
    }
};

struct ifile_replay : ifile
{
    volatile bool& can_run;
    const double speed;
    ttime_t file_time, start_time;
    std::vector<char> b, b2;

    ifile_replay(volatile bool& can_run, const std::string& fname, ttime_t tf, ttime_t tt, double speed)
        : ifile(fname, tf, tt, true), can_run(can_run), speed(speed), file_time()
    {
        start_time = cur_ttime();
    }
    uint32_t read(char* buf, uint32_t buf_size)
    {
    rep:
        uint32_t sz = b.size();
        if(buf_size - sz) {
            b2.resize(buf_size - sz);
            uint32_t r = ifile::read(&b2[0], buf_size - sz);
            if(r)
                b.insert(b.end(), b2.begin(), b2.begin() + r);
            else if(b.empty())
                return 0;
        }
        message *mb = (message*)&b[0], *me = mb + b.size() / message_size, *mi = mb;
        if(!file_time.value)
            file_time.value = mb->t.time.value;
        ttime_t t = cur_ttime();
        int64_t dt = (t - start_time) * speed;
        ttime_t f_to{file_time.value + dt};
        for(; mi != me; ++mi) {
            if(mi->t.time.value > f_to.value)
                break;
        }
        uint32_t ret = (mi - mb) * message_size;
        std::copy((char*)mb, (char*)mi, buf);
        b.erase(b.begin(), b.begin() + ret);
        if(!ret && !b.empty())
        {
            if(!can_run)
                return ret;

            int64_t s = (mb->t.time - file_time) - dt;
            assert(s >= 0);
            if(s < int64_t(11 * ttime_t::frac))
                usleep(s / 1000);
            else
            {
                sleep(10);
                start_time = cur_ttime();
                file_time.value = mb->t.time.value;
                mlog() << "ifile_replay() reinit, start_time: " << start_time << ", file_time: " << file_time;
            }
            goto rep;
        }
        return ret;
    }
};

void* ifile_create(const char* params, volatile bool& can_run)
{
    std::vector<std::string> p = split(params, ' ');
    if(p.empty() || p.size() > 5)
        throw std::runtime_error("ifile_create() [history ,replay speed ]file_name[ time_from[ time_to]]");

    bool history = (p[0] == "history");
    if(history)
        p.erase(p.begin());

    bool replay = (p[0] == "replay");
    double speed = 1.;
    if(replay) {
        speed = lexical_cast<double>(p[1]);
        p.erase(p.begin(), p.begin() + 2);
    }

    ttime_t tf = ttime_t(), tt = ttime_t();
    if(p.size() >= 2)
        tf = parse_time(p[1]);
    else if(!history && !replay)
        tf = cur_ttime();

    if(p.size() == 3)
        tt = parse_time(p[2]);
    else
        tt.value = std::numeric_limits<uint64_t>::max();

    if(replay)
        return new ifile_replay(can_run, p[0], tf, tt, speed);
    else
        return new ifile(p[0], tf, tt, history);
}

void ifile_destroy(void *v)
{
    delete ((ifile*)v);
}

uint32_t ifile_read(void *v, char* buf, uint32_t buf_size)
{
    return ((ifile*)v)->read(buf, buf_size);
}

