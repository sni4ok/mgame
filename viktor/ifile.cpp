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
#include "../evie/mlog.hpp"

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

    char_cit it = time.c_str();
    return read_time_impl().read_time<0>(it);
}

char_cit find_last(const mstring& fname, char c)
{
    char_cit it = fname.end() - 1, ib = fname.begin();
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
            return !data.empty();
        else
            return !!f.hfile;
    }
    uint32_t zlib_file_sz(const mvector<char>& f, str_holder fn)
    {
        if(f.size() < 4)
            throw mexception(es() % "zlib bad file " % fn);
        uint32_t sz = 0;
        memcpy(&sz, f.end() - 4, 4);
        if((f.size() < sz / 20) || (f.size() > 1024 && f.size() > sz / 2))
            throw mexception(es() % "zlib probably bad file " % fn);
        return sz;
    }
    void open(char_cit fname)
    {
        mlog() << "zip_file::open " << _str_holder(fname);
        close();
        str_holder fn = _str_holder(fname);
        if(fn.size() > 3 && str_holder(fn.end() - 3, fn.end()) == ".gz") {
            mvector<char> f = read_file(fname);
            uint32_t data_sz = zlib_file_sz(f, fn);
            if(!zip)
                zip.reset(new zlibe);
            zip->dest.resize(data_sz);
            data = zip->decompress(f.begin(), f.size());
            data_it = data.begin();
        }
        else {
            zip.reset();
            mfile file(fname);
            f.swap(file);
        }
    }
    void close()
    {
        if(zip)
            data = str_holder();
        else {
            mfile file(0);
            f.swap(file);
        }
    }
    void seekg(uint64_t pos)
    {
        if(zip) {
            if(pos > data.size())
                throw mexception(es() % "zip_file::seekg(), fsize " % data.size() % ", pos " % pos);
            data_it = data.begin() + pos;
        }
        else
            f.seekg(pos);
    }
    void seek_cur(int64_t pos)
    {
        if(zip)
            data_it += pos;
        else {
            if(::lseek(f.hfile, pos, SEEK_CUR) < 0)
                throw_system_failure("lseek() error");
        }
    }
    uint64_t size() const
    {
        if(zip)
            return data.size();
        else
            return f.size();
    }
    uint64_t read(char* ptr, uint64_t size)
    {
        if(zip) {
            uint64_t sz = min<uint64_t>(size, data.end() - data_it);
            memcpy(ptr, data_it, sz);
            data_it += sz;
        }
        else {
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
            else if(it->id == msg_clean) {
                orders_t& o = orders[it->mc.security_id];
                o.clear();
            }
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
                if(!!it->second.count) {
                    message m;
                    assert(!!it->second.price);
                    m.mb = it->second;
                    m.mi.time = last_time;
                    m.mi.etime = ttime_t();
                    buf.push_back(m);
                }
            }
        }

        book.resize(buf.size() * message_size);
        copy(buf.begin(), buf.begin() + buf.size(), (message*)(book.begin()));
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

    volatile bool& can_run;
    zip_file cur_file;
    compact_book cb;
    const node main_file;
    queue<mstring> dir_files;
    node nt;
    message mt;
    bool history;
    message_ping ping;
    mvector<message> read_buf;
    static constexpr ttime_t check_time = seconds(1);
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
        if(!nt.tf)
            throw mexception(es() % "time_from error for " % fname);
        nt.from = 0;
        nt.off = 0;
        nt.sz = last_file ? 0 : sz;
        cur_file.seekg(sz - message_size);
        cur_file.read((char*)&mt, message_size);
        nt.tt = mt.t.time;
        mlog() << fname << " tf: " << nt.tf.value << ", tt: " << nt.tt.value;
        if(!nt.tt || nt.tt < nt.tf)
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
        catch(exception& e) {
            mlog() << "ifile::add_file(" << fname << ") skipped, " << e;
        }
        return ttime_t();
    }

    void parse_folder(const mstring& f, uint64_t tfrom, uint64_t tto, const date& df, const date& dt,
        const mstring& dir, const uint16_t* year = nullptr)
    {
        dirent **ee;
        int n = scandir(dir.c_str(), &ee, NULL, alphasort);
        if(n == -1)
            throw_system_failure(es() % "scandir error " % dir);

        uint32_t f_size = f.size();
        for(int i = 0; i != n; ++i) {
            dirent *e = ee[i];
            str_holder fname(_str_holder(e->d_name));
            if(fname.size() > f_size + 10 && equal(fname.begin(), fname.begin() + f_size, f.begin())) {
                uint32_t t = 0;
                if(fname.size() > f_size) {
                    if(fname[f_size] != '_')
                        continue;
                    t = my_cvt::atoi<uint32_t>(fname.begin() + f_size + 1, 10);
                    if(t < tfrom)
                        continue;
                }
                dir_files.push_back(dir + fname);
                if(t && t > tto)
                    break;
            }
            else if(e->d_type == DT_DIR)
            {
                auto is_number = [](str_holder s)
                {
                    for(char c: s) {
                        if(c < '0' || c > '9')
                            return false;
                    }
                    return true;
                };

                if(year)
                {
                    if(!fname.empty() && fname.size() <= 2 && is_number(fname))
                    {
                        uint8_t month = lexical_cast<uint8_t>(fname);
                        if(date{*year, month, 0} <= dt && df <= date{*year, month, 31})
                            parse_folder(f, tfrom, tto, df, dt, dir + fname + "/");
                    }
                }
                else if(fname.size() == 4 && is_number(fname))
                {
                    uint16_t year = lexical_cast<uint16_t>(fname);
                    if(year >= df.year && year <= dt.year)
                        parse_folder(f, tfrom, tto, df, dt, dir + fname + "/", &year);
                }
            }
        }

        for(int i = 0; i != n; ++i)
            free(ee[i]);
        free(ee);
    }

    void read_directory(const mstring& fname, ttime_t tf, ttime_t tt)
    {
        char_cit it = find_last(fname, '/') + 1;
        mstring dir(fname.begin(), it);
        mstring f(it, fname.end());
        uint64_t tfrom = main_file.tf.value / ttime_t::frac, tto = main_file.tt.value / ttime_t::frac;

        date df = parse_time(tf).date, dt = parse_time(tt).date;

        parse_folder(f, tfrom, tto, df, dt, dir, nullptr);
        dir_files.push_back(fname);
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

        while(can_run && !dir_files.empty()) {
            ttime_t t = add_file(dir_files.front());
            dir_files.pop_front();
            if(!!t)
                return true;
        }
        return false;
    }

    ifile(volatile bool& can_run, const mstring& fname, ttime_t tf, ttime_t tt, bool history)
        : can_run(can_run), main_file({fname, tf, tt, 0, 0, 0}), nt(), history(history), last_file_check_time()
    {
        read_directory(fname, tf, tt);
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
            mlog() << main_file.name << " probably moved";
            last_file_ino = c;
            cur_file.open(main_file.name.c_str());
            nt.off = 0;
            return true;
        }
        return false;
    }

    uint32_t read(char_it buf, uint32_t buf_size)
    {
        if(buf_size % message_size) [[unlikely]]
            throw mexception(es() % "ifile::read(): inapropriate size: " % buf_size);
        else {
            if(!cb.book.empty())
            {
                uint32_t read_size = min<uint32_t>(buf_size, cb.book.size() - cb.book_off);
                memcpy(buf, &cb.book[0] + cb.book_off, read_size);
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

    ~ifile()
    {
    }
};

struct ifiles_replay
{
    volatile bool& can_run;
    const double speed;
    mvector<ifile> files;

    ttime_t start_time, files_time;
    queue<message> b;
    mvector<queue<char> > b2;

    ifiles_replay(volatile bool& can_run, const mvector<str_holder>& files, ttime_t tf, ttime_t tt, double speed)
        : can_run(can_run), speed(speed), files_time()
    {
        for(str_holder f: files)
            this->files.emplace_back(can_run, f, tf, tt, true);
        start_time = cur_ttime();
        b2.resize(files.size());
    }
    uint32_t read(char* buf, uint32_t buf_size)
    {
    rep:
        for(uint32_t i = 0; i != files.size(); ++i)
        {
            queue<char>& b2_ = b2[i];
            if(b2_.size() < buf_size) {
                b2_.reserve(b2_.buf_size() + 10 * buf_size);
                uint32_t r = files[i].read(b2_.end(), buf_size);
                b2_.add_size(r);
            }
        }

        uint32_t mc = buf_size / message_size;
        while(b.size() != mc)
        {
            ttime_t min_time = limits<ttime_t>::max;
            uint32_t min_idx = 0;
            for(uint32_t i = 0; i != files.size(); ++i)
            {
                queue<char>& b2_ = b2[i];
                if(b2_.size() >= message_size)
                {
                    message* m = (message*)b2_.begin();
                    if(m->t.time < min_time)
                    {
                        min_idx = i;
                        min_time = m->t.time;
                    }
                }
            }
            if(min_time == limits<ttime_t>::max)
                break;
            assert(min_time != ttime_t());

            queue<char>& b2_ = b2[min_idx];
            message* m = (message*)b2_.begin();
            b.push_back(*m);
            b2_.pop_front(message_size);
        }

        message* mb = b.begin(), *me = b.end(), *mi = mb;

        if(!files_time) [[unlikely]]
            files_time = mb->t.time;

        ttime_t t = cur_ttime();
        ttime_t dt = {int64_t((t - start_time).value * speed)};
        ttime_t f_to = files_time + dt;

        for(; mi != me; ++mi) {
            if(mi->t.time > f_to)
                break;
        }

        uint32_t ret = mi - mb;
        copy((char_cit)mb, (char_cit)mi, buf);
        b.pop_front(ret);
        if(!ret && !b.empty())
        {
            if(!can_run)
                return 0;

            ttime_t s = mb->t.time - files_time - dt;
            assert(s >= ttime_t());
            if(s < seconds(11))
                usleep(s.value / 1000);
            else
            {
                sleep(10);
                start_time = cur_ttime();
                files_time = mb->t.time;
                mlog() << "ifile_replay() reinit, start_time: " << start_time << ", files_time: " << files_time;
            }
            goto rep;
        }
        return ret * message_size;
    }
};

extern "C"
{
    void* files_replay_create(const char* params, volatile bool& can_run)
    {
        mvector<str_holder> p = split(_str_holder(params), ' ');
        if(p.empty() || p.size() > 4)
            throw str_exception("files_replay_create() speed file_name[ time_from[ time_to]]");
        double speed = lexical_cast<double>(p[0]);
        mvector<str_holder> files = split(p[1], ',');
        ttime_t tf = p.size() > 2 ? parse_time(p[2]) : limits<ttime_t>::min;
        ttime_t tt = p.size() > 3 ? parse_time(p[3]) : limits<ttime_t>::max;
        return new ifiles_replay(can_run, files, tf, tt, speed);
    }

    void files_replay_destroy(void *v)
    {
        delete ((ifiles_replay*)v);
    }

    uint32_t files_replay_read(void *v, char* buf, uint32_t buf_size)
    {
        return ((ifiles_replay*)v)->read(buf, buf_size);
    }

    void* ifile_create(const char* params, volatile bool& can_run)
    {
        mvector<str_holder> p = split(_str_holder(params), ' ');
        if(p.empty() || p.size() > 5)
            throw str_exception("ifile_create() [history ]file_name[ time_from[ time_to]]");

        bool history = (p[0] == "history");
        if(history)
            p.erase(p.begin());

        ttime_t tf = ttime_t(), tt = ttime_t();
        if(p.size() >= 2)
            tf = parse_time(p[1]);
        else if(!history)
            tf = cur_ttime();

        if(p.size() == 3)
            tt = parse_time(p[2]);
        else
            tt = limits<ttime_t>::max;

        return new ifile(can_run, p[0], tf, tt, history);
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

