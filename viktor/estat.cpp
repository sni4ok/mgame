/*
    author: Ilya Andronov <sni4ok@yandex.ru>

    export = stat
    export = stat name
*/

#include "makoa/exports.hpp"

#include <math.h>

namespace {

    struct print_t
    {
        int64_t value;
        print_t(int64_t value) :  value(value)
        {
        }
    };
    mlog& operator<<(mlog& m, print_t t)
    {
        uint64_t ta = std::abs(t.value);
        if(ta > 31536000 * my_cvt::p10<9>())
            m << (t.value / my_cvt::p10<9>() / 31536000) << " years";
        else if(ta > my_cvt::p10<10>())
            m << (t.value / my_cvt::p10<9>()) << "sec";
        else if(ta > my_cvt::p10<7>())
            m << (t.value / my_cvt::p10<6>()) << "ms";
        else if(ta > my_cvt::p10<4>())
            m << (t.value / my_cvt::p10<3>()) << "us";
        else
            m << t.value << "ns";
        return m;
    }
    using std::string;
    struct estat
    {
        std::string name;

        uint32_t count;

        struct stat
        {
            int64_t min, max, sum;
            uint64_t d2, count;

            stat() : min(std::numeric_limits<int64_t>::max()), max(std::numeric_limits<int64_t>::min()), sum(), d2(), count()
            {
            }

            void add(ttime_t f, ttime_t t)
            {
                if(f.value == uint64_t() || t.value == uint64_t())
                    return;

                int64_t delta = t.value - f.value;
                min = std::min(min, delta);
                max = std::max(max, delta);
                sum += delta;

                if(std::abs(delta) > 1000000) {
                    delta /= 1000;
                    d2 += (delta * delta);
                }
                else
                    d2 += (delta * delta) / 1000000;
                ++count;
            }
            void print(mlog& ml, string name) const
            {
                if(count) {
                    int64_t mean = sum / count;
                    double dm = double(mean) / 1000.;
                    double var = double(d2 / count) - dm * dm;
                    int64_t stddev = int64_t(sqrt(std::abs(var)) * 1000);
                    ml << "\n    " << name << " count:" << count << ", mean: "
                        << print_t(mean) << ", std: " << print_t(stddev) <<
                        ", min: " << print_t(min) << ", max: " << print_t(max);
                }
            }
        };

        //  ttime_t etime; //exchange time
        //  ttime_t time;  //parser time
        //  ttime_t mtime; //makoa server time
        //  ttime_t ctime; //cur estat time
        struct mstat
        {
            stat et, tm, mc, ec;

            void print(mlog& ml, std::string name) const
            {
                et.print(ml, name + "_et");
                tm.print(ml, name + "_tm");
                mc.print(ml, name + "_mc");
                ec.print(ml, name + "_ec");
            }
            void add(ttime_t etime, ttime_t time, ttime_t mtime, ttime_t ctime)
            {
                et.add(etime, time);
                tm.add(time, mtime);
                mc.add(mtime, ctime);
                ec.add(etime, ctime);
            }

        };
        mstat mb, mt, mc, mi, mp;

        void print() const
        {
            if(count) {
                mlog ml;
                ml << "stat" << name << " proceed " << count << " messages";
                mb.print(ml, "book");
                mt.print(ml, "trades");
                mc.print(ml, "clear");
                mi.print(ml, "instr");
                mp.print(ml, "ping");
            }
        }
        estat(std::string params) : name(params), count()
        {
            mlog() << "stat " << params << " initialized";
        }
        void proceed(const message& m)
        {
            ++count;
            ttime_t ctime = get_cur_ttime();
            if(m.id == msg_book)
                mb.add(m.mb.etime, m.mb.time, m.mtime, ctime);
            else if(m.id == msg_trade)
                mt.add(m.mt.etime, m.mt.time, m.mtime, ctime);
            else if(m.id == msg_clean)
                mc.add(ttime_t(), m.mc.time, m.mtime, ctime);
            else if(m.id == msg_instr)
                mi.add(ttime_t(), m.mi.time, m.mtime, ctime);
            else if(m.id == msg_ping)
                mp.add(ttime_t(), m.mp.time, m.mtime, ctime);
        }
        ~estat() {
            print();
        }
    };

    void* estat_init(const char* params)
    {
        return new estat(params);
    }
    void estat_destroy(void* v)
    {
        delete (estat*)(v);
    }
    void estat_proceed(void* v, const message& m)
    {
        ((estat*)(v))->proceed(m);
    }
}

extern "C"
{
    void create_hole(hole_exporter* m, simple_log* sl)
    {
        log_set(sl);
        m->init = &estat_init;
        m->destroy = &estat_destroy;
        m->proceed = &estat_proceed;
    }
}

