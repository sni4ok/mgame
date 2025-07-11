/*
    author: Ilya Andronov <sni4ok@yandex.ru>

    export = stat
    export = stat name
*/

#include "../makoa/exports.hpp"
#include "../makoa/types.hpp"

#include "../evie/math.hpp"
#include "../evie/mlog.hpp"

namespace
{
    struct estat
    {
        bool brief;
        mstring name;
        uint32_t count;

        struct stat
        {
            ttime_t min, max;
            int128_t sum, d2;
            uint64_t count;

            stat() : min(limits<ttime_t>::max), max(limits<ttime_t>::min), sum(), d2(), count()
            {
            }

            void add(ttime_t f, ttime_t t)
            {
                if(!f || !t)
                    return;

                ttime_t delta = t - f;
                min = ::min(min, delta);
                max = ::max(max, delta);
                sum += delta.value;
                d2 += delta.value * delta.value / 1000000;
                ++count;
            }
            void print(mlog& ml, mstring name) const
            {
                if(count)
                {
                    int64_t mean = sum / count;
                    double dm = double(mean) / 1000.;
                    double var = double(d2 / count) - dm * dm;
                    int64_t stddev = int64_t(sqrt(abs(var)) * 1000);
                    ml << "\n    " << name << " count: " << count << ", mean: "
                        << print_t({mean}) << ", std: " << print_t({stddev}) <<
                        ", min: " << print_t({min}) << ", max: " << print_t({max});
                }
            }
        };

        //  ttime_t etime; //exchange time
        //  ttime_t time;  //parser time
        //  ttime_t mtime; //parser out or makoa server time
        //  ttime_t ctime; //cur estat time
        struct mstat
        {
            stat et, tm, mc, tc, ec;

            void print(mlog& ml, str_holder n) const
            {
                mstring name(n);
                et.print(ml, name + "_et");
                tm.print(ml, name + "_tm");
                mc.print(ml, name + "_mc");
                tc.print(ml, name + "_tc");
                ec.print(ml, name + "_ec");
            }
            void add(ttime_t etime, ttime_t time, ttime_t mtime, ttime_t ctime)
            {
                et.add(etime, time);
                tm.add(time, mtime);
                mc.add(mtime, ctime);
                tc.add(time, ctime);
                ec.add(etime, ctime);
            }
            void add(ttime_t etime, ttime_t time)
            {
                et.add(etime, time);
            }
        };
        mstat mb, mt, mc, mi, mp;

        void print() const
        {
            if(count)
            {
                mlog ml;
                ml << "\nstat ";
                if(!name.empty())
                    ml << name << " ";
                ml << "proceed " << count << " messages";
                mb.print(ml, "book");
                mt.print(ml, "trades");
                mc.print(ml, "clear");
                mi.print(ml, "instr");
                mp.print(ml, "ping");
                ml << '\n';
            }
        }
        estat(mstring params) : brief(params == "brief"), name(params), count()
        {
            if(!params.empty())
                params.push_back(' ');

            mlog() << "stat " << params << "initialized";
        }
        void proceed(const message* mes, uint32_t count)
        {
            this->count += count;
            if(brief)
            {
                for(uint32_t i = 0; i != count; ++i, ++mes)
                {
                    const message& m = *mes;
                    if(m.id == msg_book)
                        mb.add(m.mb.etime, m.mb.time);
                    else if(m.id == msg_trade)
                        mt.add(m.mt.etime, m.mt.time);
                }
            }
            else
            {
                ttime_t mtime = get_export_mtime(mes);
                for(uint32_t i = 0; i != count; ++i, ++mes)
                {
                    const message& m = *mes;
                    ttime_t ctime = cur_ttime();
                    if(m.id == msg_book)
                        mb.add(m.mb.etime, m.mb.time, mtime, ctime);
                    else if(m.id == msg_trade)
                        mt.add(m.mt.etime, m.mt.time, mtime, ctime);
                    else if(m.id == msg_clean)
                        mc.add(m.mc.etime, m.mc.time, mtime, ctime);
                    else if(m.id == msg_instr)
                        mi.add(ttime_t(), m.mi.time, mtime, ctime);
                    else if(m.id == msg_ping)
                        mp.add(m.mp.etime, m.mp.time, mtime, ctime);
                }
            }
        }
        ~estat() {
            print();
        }
    };

    void* estat_init(char_cit params)
    {
        return new estat(_str_holder(params));
    }
    void estat_destroy(void* v)
    {
        delete (estat*)(v);
    }
    void estat_proceed(void* v, const message* m, uint32_t count)
    {
        ((estat*)(v))->proceed(m, count);
    }
}

extern "C"
{
    void create_hole(hole_exporter* m, exporter_params params)
    {
        init_exporter_params(params);
        m->init = &estat_init;
        m->destroy = &estat_destroy;
        m->proceed = &estat_proceed;
    }
}

