/*
   author: Ilya Andronov <sni4ok@yandex.ru>
*/


#include "../makoa/types.hpp"

#include "../evie/mfile.hpp"
#include "../evie/fset.hpp"
#include "../evie/signals.hpp"
#include "../evie/mlog.hpp"

#include <unistd.h>
#include <dirent.h>

void fix_zero_tail(char_cit fname)
{
    cout() << "fix_zero_tail(" << _str_holder(fname) << "):";
    mfile f(fname);
    uint64_t fsz = f.size();
    mvector<char> m(message_size), mc(message_size);
    int64_t nsz = fsz - fsz % message_size;
    while(nsz > 0) {
        f.seekg(nsz - message_size);
        f.read(m.begin(), message_size);
        if(!(m == mc))
            break;
        nsz -= message_size;
    }
    cout() << "  orig_file_size: " << fsz << ", new_file_size: " << nsz;
    if(truncate(fname, nsz))
        throw str_exception("truncate file error");
}

void sort_data_by_folders(str_holder folder)
{
    if(folder[folder.size() - 1] != '/')
        throw mexception(es() % "sort_data_by_folders() bad folder name: " % folder);

    cout() << "sort_data_by_folders(" << folder << "):";
    mvector<mstring> files_for_move;
    {
        dirent **ee;
        int n = scandir(folder.begin(), &ee, NULL, alphasort);
        if(n == -1)
            throw_system_failure(es() % "scandir error " % folder);
        for(int i = 0; i != n; ++i) {
            dirent *e = ee[i];
            if(e->d_type == DT_REG)
            {
                str_holder fname(_str_holder(e->d_name));
                if(fname.size() < 3 || str_holder(fname.end() - 3, fname.end()) != ".gz")
                    throw mexception(es() %
                        "sort_data_by_folders() unsupported file type: " % fname);
                files_for_move.push_back(fname);
            }
        }
        for(int i = 0; i != n; ++i)
            free(ee[i]);
        free(ee);
    }

    fset<mstring> created_dirs;
    for(const mstring& fname: files_for_move)
    {
        char_cit e = fname.end() - 3;
        char_cit f = find(fname.begin(), e, '_');
        if(f == e || (e - f) != 11)
            throw mexception(es() % "sort_data_by_folders() unsupported file: " % fname);
        uint32_t t = lexical_cast<uint32_t>(f + 1, e);
        date d = parse_time(seconds(t)).date;
        mstring nf = folder + to_string(d.year) + "/" + to_string(d.month) + "/";
        if(created_dirs.find(nf) == created_dirs.end())
        {
            create_directories(nf.c_str());
            created_dirs.insert(nf);
        }
        rename_file((folder + fname).c_str(), (nf + fname).c_str());
    }
    cout() << "sort_data_by_folders successfully ended, moved "
        << files_for_move.size() << " files";
}

template<typename type>
void test_io(type c)
{
    char buf[128];
    buf_stream str(buf);
    str << c;
    type v = lexical_cast<type>(str.str());
    assert(v == c);
    unused(v);
}

void test_impl(str_holder a1, str_holder a2)
{
    count_t v1 = lexical_cast<count_t>(a1);
    count_t v2 = lexical_cast<count_t>(a2);
    test_io(v1);
    assert(v1.value == v2.value);
    test_io(v1);
    unused(v1, v2);
}

void amount_test()
{
    test_impl("-0.5E-50", "0");
    test_impl("5.6", "0.56E1");
    test_impl("0.056", "0.56E-1");
    test_impl("0.56", "5.6E-1");
    test_impl("-0.056", "-0.56E-1");
    test_impl("-5.6", "-0.56E1");
    test_impl("-0.56", "-5.6E-1");
    test_impl("560", "56E1");
    test_impl("5e-7", "0.5E-6");

    str_holder v = "9.79380846343861E-4";
    count_t c = lexical_cast<count_t>(v);
    unused(c);

    test_io(price_t({limits<int64_t>::max}));
    test_io(price_t({limits<int64_t>::min}));

    test_io(count_t({limits<int64_t>::max}));
    test_io(count_t({limits<int64_t>::min}));

    cout() << "amount_test successfully ended";
}

void clear_screen()
{
    cout(false) << "\033[2J\033[1;1H";
}

void parsers_stat(str_holder f)
{
    auto log = log_init();
    signals_holder sl;
    mvector<mstring> files = split_s(f);

    struct st
    {
        ttime_t from;
        uint64_t from_size;
        uint64_t size;
    };

    mvector<st> stats(files.size());
    ttime_t ct;

    auto update_stat = [&]()
    {
        for(uint32_t i = 0; i != files.size(); ++i)
        {
            st& s = stats[i];
            uint64_t sz;

            if(is_file_exist(files[i].c_str(), &sz))
            {
                if(!s.from || sz < s.size)
                {
                    s.from = ct;
                    s.from_size = sz;
                }

                s.size = sz;
            }
            else
                s = st();
        }
    };

    while(can_run)
    {
        ct = cur_mtime();
        update_stat();
        ct = cur_mtime();
        clear_screen();

        for(uint32_t i = 0; i != files.size(); ++i)
        {
            st& s = stats[i];
            ttime_t d = ct - s.from;

            if(!!s.from)
            {
                uint64_t sz = (s.size - s.from_size) / message_size;
                uint64_t mps = sz * ttime_t::frac / d.value;
                cout(false) << uint_fixed<7, false>(mps) << "/s " << files[i];
            }
            else
                cout(false) << "[no data] " << files[i];
        }

        usleep(500000);
    }
}

int main(int argc, char** argv)
{
    try
    {
        if(argc == 3 && _str_holder(argv[1]) == "fix_zero_tail")
            fix_zero_tail(argv[2]);
        else if(argc == 3 && _str_holder(argv[1]) == "sort_data_by_folders")
            sort_data_by_folders(_str_holder(argv[2]));
        else if(argc == 2 && _str_holder(argv[1]) == "amount_test")
            amount_test();
        else if(argc == 3 && _str_holder(argv[1]) == "parsers_stat")
            parsers_stat(_str_holder(argv[2]));
        else
            throw str_exception("unsupported params");
    }
    catch(exception& e)
    {
        cerr() << "main() exception: " << _str_holder(e.what());
        return 1;
    }
    return 0;
}

