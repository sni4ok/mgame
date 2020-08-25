/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "evie/mfile.hpp"
#include "evie/utils.hpp"
#include "makoa/types.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

static ttime_t parse_time(const std::string& time)
{
    const char* it = time.c_str();
    return read_time_impl().read_time<0>(it);
}

struct ifile
{
    int cur_file;
    ttime_t cur_t;

    struct node
    {
        std::string name;
        ttime_t tf, tt;
        uint32_t sz;
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
    void add_file_impl(const std::string& fname)
    {
        mfile f(fname.c_str(), false);
        uint64_t sz = f.size();
        if(sz < message_size)
            return;
        sz = sz - sz % message_size;
        f.read((char*)&mt, message_size);
        nt.tf = mt.t.time;
        if(!nt.tf.value)
            throw std::runtime_error(es() % "time_from error for " % fname);
        nt.sz = sz;
        f.seekg(sz - message_size);
        f.read((char*)&mt, message_size);
        nt.tt = mt.t.time;
        if(!nt.tt.value || nt.tt.value < nt.tf.value)
            throw std::runtime_error(es() % "time_to error for " % fname);
        if(main_file.crossed(nt)) {
            nt.name = fname;
            files.push_back(nt);
        }
    }

    void add_file(const std::string& fname)
    {
        try {
            add_file_impl(fname);
        }
        catch(std::exception& e) {
            mlog() << "ifile::add_file(" << fname << ") skipped, " << e;
        }
    }

    void read_directory(const std::string& fname)
    {
        std::string::const_iterator it = fname.end() - 1, ib = fname.begin();
        while(it != ib && *it != '/')
            --it;
        ++it;

        std::string dir(ib, it);
        std::string f(it, fname.end());

        DIR *d = opendir(dir.c_str());
        if(!d)
            throw_system_failure(es() % "list directory error " % dir);
        dirent *e;
        while((e = readdir(d)))
        {
            _str_holder fname(e->d_name);
            uint32_t fc = std::min<uint32_t>(fname.size, f.size());
            if(std::equal(fname.str, fname.str + fc, f.begin()))
                add_file(dir + e->d_name);
        }
        closedir(d);
        std::sort(files.begin(), files.end());
    }

    ifile(const std::string& fname, ttime_t tf, ttime_t tt)
        : cur_file(), cur_t(), main_file({fname, tf, tt, 0})
    {
        read_directory(fname);
    }

    uint32_t read(char* buf, uint32_t buf_size)
    {
        if(unlikely(buf_size % message_size))
            throw std::runtime_error(es() % "ifile::read(): inapropriate size: " % buf_size);
        else {
            uint32_t ret = 0;
            while(!ret)
            {
                if(!cur_file) {
                    if(files.empty())
                        return 0;
                    
                    nt = *files.rbegin();
                    files.pop_back();
                    cur_file = ::open(nt.name.c_str(), O_RDONLY);
                    //mlog() << "open " << nt.name;
                }
                ret = ::read(cur_file, buf, buf_size);
                if(!ret) {
                    if(!files.empty()) {
                        close(cur_file);
                        cur_file = 0;
                    }
                    else
                        return ret;
                }
            }
            return ret;
        }
    }

    ~ifile() {
        if(cur_file)
            close(cur_file);
    }
};

void* ifile_create(const char* params)
{
    std::vector<std::string> p = split(params, ' ');
    if(p.empty() || p.size() > 3)
        throw std::runtime_error("ifile_create() file_name[ time_from[ time_to]]");
    ttime_t tf = ttime_t(), tt = ttime_t();
    if(p.size() >= 2)
        tf = parse_time(p[1]);
    else
        tf = get_cur_ttime();

    if(p.size() == 3)
        tt = parse_time(p[2]);
    else
        tt.value = std::numeric_limits<uint64_t>::max();

    return new ifile(p[0], tf, tt);
}

void ifile_destroy(void *v)
{
    delete ((ifile*)v);
}

uint32_t ifile_read(void *v, char* buf, uint32_t buf_size)
{
    return ((ifile*)v)->read(buf, buf_size);
}

