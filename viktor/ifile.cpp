/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mfile.hpp"
#include "evie/utils.hpp"
#include "evie/profiler.hpp"
#include "makoa/types.hpp"

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
    uint64_t readed;

    std::vector<char> book;
    uint64_t book_off;
    ttime_t last_time;

    compact_book() : readed(), book_off(), last_time()
    {
    }

    struct orders_t : std::map<int64_t/*level_id*/, message_book>
    {
        message_instr mi;
    };
    std::map<uint32_t/*security_id*/, orders_t> orders;

    void read(const std::string& fname, ttime_t tf)
    {
        uint32_t buf_size = 1024;
        std::vector<message> buf(buf_size);

        int f = ::open(fname.c_str(), O_RDONLY);
        if(!f)
            throw std::runtime_error(es() % "open file " % fname % " error");
        for(;;) {
            int32_t sz = ::read(f, (char*)&buf[0], message_size * buf_size);
            if(sz < 0)
                throw_system_failure(es() % "read file " % fname % " error");
            auto it = buf.begin(), ie = it + (sz / message_size);
            for(; it != ie; ++it) {
                if(it->mt.time.value >= tf.value)
                    break;

                if(it->id == msg_book) {
                    const message_book& m = it->mb;
                    message_book& mb = (orders[m.security_id])[m.level_id];
                    price_t price = m.price.value ? m.price : mb.price;
                    mb = m;
                    mb.price = price;
                }
                else if(it->id == msg_instr) {
                    orders_t& o = orders[it->mi.security_id];
                    o.clear();
                    o.mi = it->mi;
                }
                else if(it->id == msg_clean)
                    orders[it->mc.security_id].clear();

                readed += message_size;
                last_time = it->mt.time;
            }
            if(it != ie || ie != buf.end() || sz % message_size)
                break;
        }
        ::close(f);
        compact();
    }

    void compact()
    {
        std::vector<message> buf;
        auto i = orders.begin(), e = orders.end();
        for(; i != e; ++i) {
            message m;
            m.mi = i->second.mi;
            m.mt.time = last_time;
            m.mt.etime = ttime_t();
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
                    m.mt.time = last_time;
                    m.mt.etime = ttime_t();
                    buf.push_back(m);
                }
            }
        }
        std::sort(buf.begin() + orders.size(), buf.end(), [](const message& l, const message& r) {return l.mb.time.value < r.mb.time.value;});
        book.resize(buf.size() * message_size);
        std::copy(&buf[0], &buf[0] + buf.size(), (message*)&book[0]);
    }
};

struct ifile
{
    int cur_file;
    compact_book cb;

    struct node
    {
        std::string name;
        ttime_t tf, tt;
        uint64_t off, sz;
        bool operator<(const node& n) const
        {
            return tf > n.tf;
        }
        bool crossed(const node& n) const
        {
            return (tt > n.tf && n.tt > tf);
        }
    };

    const node main_file;
    std::vector<node> files;

    node nt;
    message mt;

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

    ttime_t add_file_impl(const std::string& fname, bool last_file)
    {
        mfile f(fname.c_str());
        uint64_t sz = f.size();
        if(sz < message_size)
            return ttime_t();
        sz = sz - sz % message_size;
        f.read((char*)&mt, message_size);
        nt.tf = mt.t.time;
        if(!nt.tf.value)
            throw std::runtime_error(es() % "time_from error for " % fname);
        nt.off = 0;
        nt.sz = sz;
        f.seekg(sz - message_size);
        f.read((char*)&mt, message_size);
        nt.tt = mt.t.time;
        if(!nt.tt.value || nt.tt.value < nt.tf.value)
            throw std::runtime_error(es() % "time_to error for " % fname);
        if(main_file.crossed(nt) || (!history && files.empty() && last_file)) {
            nt.name = fname;
            if(nt.tt > main_file.tt) {
                nt.sz = message_size * lower_bound_int(nt.off / message_size, nt.sz / message_size, main_file.tt,
                    [&f](int64_t off, ttime_t tt) {
                        message m;
                        f.seekg(off * message_size);
                        f.read((char*)&m, message_size);
                        return m.mt.time < tt;
                    }
                );
            }
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
            cb.read(it->name, main_file.tf);
            it->off = cb.readed;
            it->sz -= cb.readed;
        }
    }

    bool history;
    message_ping ping;
    std::vector<char> mc;

    ifile(const std::string& fname, ttime_t tf, ttime_t tt, bool history)
        : cur_file(), main_file({fname, tf, tt, 0, false}), history(history), mc(message_size)
    {
        read_directory(fname);
        ping.id = msg_ping;
    }

    uint32_t read(char* buf, uint32_t buf_size)
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
                if(!history && !nt.sz && files.empty()) {
                    ret = ::read(cur_file, buf, buf_size);
                    while(ret > int32_t(buf_size)) {
                        if(std::equal(&buf[ret - buf_size], &buf[ret], &mc[0]))
                            ret -= buf_size;
                        else
                            break;
                    }
                }
                else
                    ret = ::read(cur_file, buf, std::min<uint64_t>(buf_size, nt.sz));
                if(ret < 0)
                    throw_system_failure("ifile::read file error");
                else if(!ret) {
                    if(!files.empty()) {
                        close(cur_file);
                        cur_file = 0;
                    }
                    else if(history)
                        return ret;
                    else {
                        usleep(10 * 1000);
                        ping.time = get_cur_ttime();
                        if(ping.time > main_file.tt)
                            return ret;
                        memcpy(buf, &ping, message_size);
                        return message_size;
                    }
                }
                else
                    nt.sz -= ret;
            }
            return ret;
        }
    }

    ~ifile()
    {
        if(cur_file)
            close(cur_file);
    }
};

void* ifile_create(const char* params)
{
    std::vector<std::string> p = split(params, ' ');
    if(p.empty() || p.size() > 4)
        throw std::runtime_error("ifile_create() [history ]file_name[ time_from[ time_to]]");

    bool history = (p[0] == "history");
    if(history)
        p.erase(p.begin());

    ttime_t tf = ttime_t(), tt = ttime_t();
    if(p.size() >= 2)
        tf = parse_time(p[1]);
    else
        tf = get_cur_ttime();

    if(p.size() == 3)
        tt = parse_time(p[2]);
    else
        tt.value = std::numeric_limits<uint64_t>::max();

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

