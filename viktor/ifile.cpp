/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "../makoa/types.hpp"

#include "../alco/huobi/utils.hpp"

#include "../evie/fmap.hpp"
#include "../evie/mfile.hpp"
#include "../evie/utils.hpp"
#include "../evie/queue.hpp"
#include "../evie/profiler.hpp"

#include <map>
#include <unordered_map>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static ttime_t parse_time(const mstring& time)
{
    if(time == "now")
        return cur_ttime();

    const char* it = time.c_str();
    return read_time_impl().read_time<0>(it);
}

mstring::const_iterator find_last(const mstring& fname, const char c)
{
    auto it = fname.end() - 1, ib = fname.begin();
    while(it != ib && *it != c)
        --it;
    return it;
}

struct zip_file
{
    str_holder data;
    unique_ptr<zlibe> zip;
    const char* data_it;
    mfile f;

    zip_file() : f(0)
    {
    }
    operator bool() const
    {
        if(zip)
            return !!data.size;
        else
            return !!f.hfile;
    }
    void open(const char* fname)
    {
        mlog() << "zip_file::open " << _str_holder(fname);
        close();
        str_holder fn = _str_holder(fname);
        if(fn.size > 3 && str_holder(fn.end() - 3, fn.end()) == ".gz")
        {
            mvector<char> f = read_file(fname);
            uint64_t alloc = max<uint64_t>(10 * f.size(), 1024);
            if(!zip)
                zip.reset(new zlibe);
            zip->buf.resize(4 * alloc);
            zip->dest.resize(alloc);
            data = zip->decompress(f.begin(), f.size());
            data_it = data.begin();
        }
        else
        {
            zip.reset();
            mfile file(fname);
            f.swap(file);
        }
    }
    void close()
    {
        if(zip)
            data = str_holder();
        else
        {
            mfile file(0);
            f.swap(file);
        }
    }
    void seekg(uint64_t pos)
    {
        if(zip)
        {
            if(pos > data.size)
                throw mexception(es() % "zip_file::seekg(), fsize " % data.size % ", pos " % pos);
            data_it = data.begin() + pos;
        }
        else
            f.seekg(pos);
    }
    void seek_cur(int64_t pos)
    {
        if(zip)
            data_it += pos;
        else
        {
            if(::lseek(f.hfile, pos, SEEK_CUR) < 0)
                throw_system_failure("lseek() error");
        }
    }
    uint64_t size() const
    {
        if(zip)
            return data.size;
        else
            return f.size();
    }
    uint64_t read(char* ptr, uint64_t size)
    {
        if(zip)
        {
            uint64_t sz = min<uint64_t>(size, data.end() - data_it);
            my_fast_copy(data_it, data_it + sz, ptr);
            data_it += sz;
        }
        else
        {
            ssize_t r = ::read(f.hfile, ptr, size);
            if(r < 0)
                throw_system_failure(es() % "read() error, size: " % size);
            return r;
        }
        return size;
    }
};

template<typename map>
class last_used_c : public map
{
    typedef typename map::key_type key_type;
    typedef typename map::mapped_type value_type;

    key_type last;
    value_type* v;

public:

    last_used_c() : last(), v() {
    }
    value_type& operator[](const key_type& key) {
        if(key == last)
            return *v;
        map* m = this;
        v = &(*m)[key];
        last = key;
        return *v;
    }
    void erase(const key_type& key) {
        last = key_type();
        ((map*)this)->erase(key);
    }
};

struct compact_book
{
    mvector<char> book;
    uint64_t book_off;

    compact_book() : book_off()
    {
    }

    struct orders_t : std::unordered_map<int64_t/*level_id*/, message_book>
    {
        message_instr mi;

        orders_t() : mi()
        {
        }
    };

    void read(zip_file& f, const mstring& fname, uint64_t from, uint64_t to)
    {
        if((to - from) % message_size)
            throw mexception(es() % "compact_book::read() " % fname % " from " % from % " to " % to);

        ttime_t last_time = ttime_t();
        last_used_c<std::map<uint32_t, orders_t> > orders;

        //read
        mvector<message> buf((to - from) / message_size);
        f.seekg(from);
        f.read((char*)(buf.begin()), to - from);
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
        std::copy(buf.begin(), buf.begin() + buf.size(), (message*)(book.begin()));
        mlog() << "compact_book::read() " << fname << " from " << from << " to " << to;
    }
};

struct ifile
{
    struct node
    {
        mstring name;
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

    zip_file cur_file;
    compact_book cb;
    const node main_file;
    queue<mstring> dir_files;
    node nt;
    message mt;
    bool history;
    message_ping ping;
    mvector<message> read_buf;
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

    ttime_t add_file_impl(const mstring& fname)
    {
        bool last_file = fname == main_file.name;

        cur_file.open(fname.c_str());
        if(last_file && !cur_file.zip)
            last_file_ino = file_ino(cur_file.f.hfile);
        uint64_t sz = cur_file.size();
        if(sz < message_size)
            return ttime_t();
        sz = sz - sz % message_size;
        cur_file.read((char*)&mt, message_size);
        nt.tf = mt.t.time;
        if(!nt.tf.value)
            throw mexception(es() % "time_from error for " % fname);
        nt.from = 0;
        nt.off = 0;
        nt.sz = last_file ? 0 : sz;
        cur_file.seekg(sz - message_size);
        cur_file.read((char*)&mt, message_size);
        nt.tt = mt.t.time;
        mlog() << "" << fname << " tf: " << nt.tf.value << ", tt: " << nt.tt.value;
        if(!nt.tt.value || nt.tt.value < nt.tf.value)
            throw mexception(es() % "time_to error for " % fname);

        if(main_file.crossed(nt) || (!history && last_file)) {
            nt.name = fname;
            auto pred =
                [&](int64_t off, ttime_t tt) {
                    message m;
                    cur_file.seekg(off * message_size);
                    cur_file.read((char*)&m, message_size);
                    return m.mt.time < tt;
                };

            if(nt.tt > main_file.tf || last_file) {
                assert(nt.from == 0 && nt.off == 0);
                static const uint64_t buf_size = message_size * 1024 * 1024;
                read_buf.resize(buf_size / message_size);
                nt.off = message_size * lower_bound_int(0, sz / message_size, main_file.tf, pred);
                last_used_c<fmap<uint32_t/*security_id*/, uint64_t /*off*/> > tickers;
                uint64_t off = nt.off;

                while(!nt.from && off) {
                    nt.from = off < buf_size ? 0 : off - buf_size;
                    cur_file.seekg(nt.from);
                    cur_file.read((char*)&read_buf[0], off - nt.from);
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
                        nt.from = min(nt.from, v.second);
                }
            }

            if((nt.tt > main_file.tt) || (last_file && history))
                nt.sz = message_size * lower_bound_int(nt.off / message_size, sz / message_size, main_file.tt, pred);

            cur_file.seekg(nt.off);

            mlog() << "ifile::add_file_impl() " << fname << ", last: " << last_file << ", from: "
                << nt.from << ", off: " << nt.off << ", sz: " << nt.sz;

            return nt.tt;
        }
        return ttime_t();
    }

    ttime_t add_file(const mstring& fname)
    {
        try {
            return add_file_impl(fname);
        }
        catch(std::exception& e) {
            mlog() << "ifile::add_file(" << fname << ") skipped, " << e;
        }
        return ttime_t();
    }

    void read_directory(const mstring& fname)
    {
        MPROFILE("ifile::read_directory")
        mstring::const_iterator it = find_last(fname, '/') + 1;
        mstring dir(fname.begin(), it);
        mstring f(it, fname.end());

        dirent **ee;
        int n = scandir(dir.c_str(), &ee, NULL, alphasort);
        if(n == -1)
            throw_system_failure(es() % "scandir error " % dir);

        uint32_t f_size = f.size();
        uint64_t tfrom = main_file.tf.value / ttime_t::frac, tto = main_file.tt.value / ttime_t::frac;
        for(int i = 0; i != n; ++i) {
            dirent *e = ee[i];
            str_holder fname(_str_holder(e->d_name));
            if(fname.size > f_size + 10 && std::equal(fname.str, fname.str + f_size, f.begin())) {
                uint32_t t = 0;
                if(fname.size > f_size) {
                    if(fname.str[f_size] != '_')
                        continue;
                    t = my_cvt::atoi<uint32_t>(fname.str + f_size + 1, 10);
                    if(t < tfrom)
                        continue;
                }
                dir_files.push_back(dir + fname);
                if(t && t > tto)
                    break;
            }
        }
        dir_files.push_back(fname);

        for(int i = 0; i != n; ++i)
            free(ee[i]);
        free(ee);
    }

    void load_cb()
    {
        if(next_file())
            cb.read(cur_file, nt.name, nt.from, nt.off);
    }

    bool next_file()
    {
        if(nt.tt > main_file.tt)
            return false;

        while(!dir_files.empty())
        {
            ttime_t t = add_file(dir_files.front());
            dir_files.pop_front();
            if(t.value)
                return true;
        }
        return false;
    }

    ifile(const mstring& fname, ttime_t tf, ttime_t tt, bool history)
        : main_file({fname, tf, tt, 0, 0, 0}), nt(), history(history), last_file_check_time()
    {
        read_directory(fname);
        load_cb();
        ping.id = msg_ping;
    }

    bool last_file_moved()
    {
        if(cur_file.zip)
            return false;

        int f = ::open(main_file.name.c_str(), O_RDONLY);
        if(f < 0)
            return false;
        uint64_t sz = 0;
        ino_t c = file_ino(f, &sz);
        ::close(f);
        if(c != last_file_ino && sz >= message_size)
        {
            mlog() << "" << main_file.name << " probably moved";
            last_file_ino = c;
            cur_file.open(main_file.name.c_str());
            nt.off = 0;
            return true;
        }
        return false;
    }

    virtual uint32_t read(char* buf, uint32_t buf_size)
    {
        if(unlikely(buf_size % message_size))
            throw mexception(es() % "ifile::read(): inapropriate size: " % buf_size);
        else {
            if(!cb.book.empty())
            {
                uint32_t read_size = min<uint32_t>(buf_size, cb.book.size() - cb.book_off);
                my_fast_copy(&cb.book[0] + cb.book_off, &cb.book[0] + cb.book_off + read_size, buf);
                cb.book_off += read_size;
                if(cb.book_off == cb.book.size())
                    cb.book.clear();
                return read_size;
            }
            int32_t ret = 0;
            while(!ret)
            {
                if(!cur_file && !next_file())
                    return 0;

                if(!history && !nt.sz && dir_files.empty())
                    ret = cur_file.read(buf, buf_size);
                else
                    ret = cur_file.read(buf, min<uint64_t>(buf_size, nt.sz - nt.off));

                if(ret > 0)
                {
                    if(ret % message_size)
                    {
                        MPROFILE("ifile::fix_read_size()")
                        int32_t d = (ret % message_size);
                        ret = ret - d;
                        cur_file.seek_cur(-d);
                    }
                    nt.off += ret;
                }
                if(ret < 0)
                    throw_system_failure("ifile::read file error");
                else if(!ret)
                {
                    if(!dir_files.empty())
                        cur_file.close();
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
    }
};

struct ifile_replay : ifile
{
    volatile bool& can_run;
    const double speed;
    ttime_t file_time, start_time;
    mvector<char> b, b2;

    ifile_replay(volatile bool& can_run, const mstring& fname, ttime_t tf, ttime_t tt, double speed)
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
                b.insert(b2.begin(), b2.begin() + r);
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
        my_fast_copy((char*)mb, (char*)mi, buf);
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

extern "C"
{
    void* ifile_create(const char* params, volatile bool& can_run)
    {
        mvector<mstring> p = split(_mstring(params), ' ');
        if(p.empty() || p.size() > 5)
            throw str_exception("ifile_create() [history ,replay speed ]file_name[ time_from[ time_to]]");

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
}

