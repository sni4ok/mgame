/*
    shared library consumers for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "exports.hpp"
#include "types.hpp"
#include "mmap.hpp"

#include "../tyra/tyra.hpp"

#include "../evie/profiler.hpp"
#include "../evie/smart_ptr.hpp"
#include "../evie/fmap.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>

volatile bool* can_run_impl = nullptr;

void set_can_run(volatile bool* can_run)
{
    can_run_impl = can_run;
}

void init_exporter_params(exporter_params params)
{
    log_set(params.sl);
    set_can_run(params.can_run);
}

namespace
{
    void log_message(void*, const message* m, uint32_t count)
    {
        for(uint32_t i = 0; i != count; ++i, ++m) {
            if(m->id == msg_book)
                mlog() << "<" << m->mb;
            else if(m->id == msg_trade)
                mlog() << "<" << m->mt;
            else if(m->id == msg_clean)
                mlog() << "<" << m->mc;
            else if(m->id == msg_instr)
                mlog() << "<" << m->mi;
            else if(m->id == msg_ping)
                mlog() << "<" << m->mp;
            else if(m->id == msg_hello)
                mlog() << "<" << m->dratuti;
        }
        mlog() << "<flush|";
    }
    void* hole_no_init(const char*)
    {
        return 0;
    }
    void hole_no_destroy(void*)
    {
    }
    void hole_no_proceed(void*, const message*, uint32_t)
    {
    }
    void* tyra_create(const char* params)
    {
        return new tyra(params);
    }
    void tyra_destroy(void* t)
    {
        delete (tyra*)t;
    }
    void tyra_proceed(void* t, const message* m, uint32_t count)
    {
        return ((tyra*)t)->send(m, count);
    }
    struct exports_chain
    {
        static const uint32_t max_size = 20;
        exporter exporters[max_size];
        uint32_t size;

        exports_chain() : size()
        {
        }
        void proceed(const message* m, uint32_t count)
        {
            for(uint32_t i = 0; i != size; ++i)
                exporters[i].proceed(m, count);
        }
        void add(exporter&& e)
        {
            if(size == max_size)
                throw str_exception("exports_chain max_size exceed");
            exporters[size++] = std::move(e);
        }
    };
    void chain_destroy(void* ptr)
    {
        delete (exports_chain*)ptr;
    }
    void chain_proceed(void* ec, const message* m, uint32_t count)
    {
        ((exports_chain*)ec)->proceed(m, count);
    }

    exporter shared_exporter(const mstring& module);

    struct exports_factory : noncopyable
    {
        fmap<mstring, hole_exporter> exporters;
        mvector<exporter> dyn_exporters;

        uint32_t register_exporter(const mstring& module, hole_exporter he)
        {
            auto it = exporters.find(module);
            if(it != exporters.end())
                return 0;
                //throw mexception(es() % "exports_factory() double registration for " % module);
            exporters[module] = he;
            return exporters.size();
        }
        hole_exporter get(const mstring& module)
        {
            auto it = exporters.find(module);
            if(it != exporters.end())
                return it->second;
            dyn_exporters.push_back(shared_exporter(module));
            exporter& e = dyn_exporters.back();
            hole_exporter he = e.he;
            exporters[module] = he;
            return he;
        }
    };

    static exports_factory efactory;
    
    void shared_destroy(void* handle)
    {
        if(handle)
            dlclose(handle);
    }
    exporter shared_exporter(const mstring& module)
    {
        mstring lib = "./lib" + module + ".so";
        exporter ret;
        ret.he.destroy = &shared_destroy;
        ret.p = dlopen(lib.c_str(), RTLD_NOW);
        if(!ret.p)
            throw_system_failure(es() % "load library '" % lib % "' error");
        typedef void (create_hole)(hole_exporter* m, exporter_params params);
        create_hole *hole = (create_hole*)dlsym(ret.p, "create_hole");
        if(!hole)
            throw_system_failure(es() % "not found create_hole() in '" % lib % "'");

        efactory.dyn_exporters.push_back(std::move(ret));
        exporter e;
        assert(can_run_impl);
        hole(&e.he, {log_get(), can_run_impl});
        return e;
    }
    exporter create_impl(const mstring& m)
    {
        auto ib = m.begin(), ie = m.end(), it = find(ib, ie, ' ');
        mstring module, params;
        if(it == ie || it + 1 == ie)
            module = m;
        else {
            module = mstring(ib, it);
            params = mstring(it + 1, ie);
        }
        exporter ret;
        ret.he = efactory.get(module);
        ret.p = ret.he.init(params.c_str());
        return ret;
    }
    void* pipe_init(const char* params)
    {
        int r = mkfifo(params, 0666);
        my_unused(r);
        int64_t h = ::open(params, O_WRONLY);
        if(h <= 0)
            throw_system_failure(es() % "open " % _str_holder(params) % " error");

        return (void*)h;
    }
    void pipe_destroy(void* p)
    {
        ::close(int((int64_t)p));
    }
    void pipe_proceed(void* p, const message* m, uint32_t count)
    {
        uint32_t wsize = sizeof(message) * count;
        ssize_t ret = ::write(int((int64_t)p), (const void*)m, wsize);
        if(ret != wsize) [[unlikely]]
            throw_system_failure(es() % "pipe::write, wsize: " % wsize  % ", ret: " % ret);
    }
    void* mmap_init(const char* params)
    {
        mlog() << "mmap_init() open: " << _str_holder(params);
        void *p = mmap_create(params, false);
        unique_ptr<void, mmap_close> ptr(p);
        shared_memory_sync* s = get_smc(p);

        uint8_t* c = (uint8_t*)p;
        {
            char& s = *((char*)p + mmap_alloc_size);
            if(s != '1')
            {
                mmap_store(c, 1);
                mmap_store(c + 1, 0);
                throw mexception(es() % "mmap_init() mmap already open: " % s);
            }
            ++s;
        }
        pthread_lock lock(s->mutex);
        for(uint8_t* s = c; s != c + mmap_alloc_size_base; ++s)
        {
            if(*s)
                throw mexception(es() % "mmap_init() mmap already filled: " % _str_holder(params));
        }
        *c = 2;
        *(c + 1) = 1;

        while(*(c + 1) != 2 && *can_run_impl)
        {
            if(mmap_nusers(params) != 2)
                throw str_exception("import_mmap_cp_init() nusers != 2");
            pthread_cond_signal(&(s->condition));
            pthread_timedwait(s->condition, s->mutex, 1);
        }
        mlog() << "mmap_init() successfully opened: " << _str_holder(params);
        return ptr.release();
    }
    void mmap_destroy(void *p)
    {
        mmap_close(p);
    }
    void mmap_proceed(void* v, const message* m, uint32_t count)
    {
        bool flub = false;
        uint8_t* f = (uint8_t*)v, *e = f + message_size, *i = f;

        uint8_t w = *f;
        uint8_t r = *(f + 1);
        if(w < 2 || w >= message_size || !r || r >= message_size) [[unlikely]]
            throw mexception(es() % "mmap_proceed() internal error, wp: " % uint32_t(w) % ", rp: " % uint32_t(r));

        i += w;
        message* p = (((message*)v) + 1) + (w - 2) * 255;

        uint32_t c = 0;
        while(c != count)
        {
            uint32_t cur_count = (count - c);
            if(cur_count > 255)
                cur_count = 255;

            uint8_t nf = mmap_load(i);
            if(nf) {
                //reader not exists or overloaded
                time_t tf = time(NULL);
                flub = true;
                while(nf && tf + 5 >= time(NULL)){
                    usleep(10);
                    nf = mmap_load(i);
                }
                if(nf)
                    throw mexception(es() % "mmap_proceed() map overload, wp: " % uint32_t(*f) % ", rp: " % uint32_t(*(f + 1)));
            }
            memcpy(p, m + c, cur_count * message_size);
            mmap_store(i, cur_count);
            //mlog() << "mmap_proceed(" << uint64_t(v) << "," << uint64_t(p) << "," << c << "," << cur_count
            //    << "," << uint32_t(*f) << "," << uint32_t(*(f + 1)) << ")";
            p += 255;
            ++i;
            if(i == e)
                i = f + 2;
            if(!mmap_compare_exchange(f, w, i - f))
                throw mexception(es() % "mmap_proceed() set w error, w: " % *f % ", from: " % uint32_t(w) % ", to: " % uint32_t(i - f));
            w = i - f;
            c += cur_count;
        }

        if(flub)
        {
            MPROFILE("mmap_proceed_flub")
        }

        shared_memory_sync* s = get_smc(v);
        if(s->pooling_mode == 2 && !pthread_mutex_lock(&(s->mutex))) {
            pthread_cond_signal(&(s->condition));
            pthread_mutex_unlock(&(s->mutex));
        }
    }
}

uint32_t register_exporter(str_holder module, hole_exporter he)
{
    return efactory.register_exporter(module, he);
}

exporter::exporter(const mstring& params)
{
    mvector<mstring> exports = split(params, ';');
    if(params.empty())
        throw str_exception("create_exporter() with empty params");
    if(exports.size() == 1)
        (*this) = create_impl(exports[0]);
    else {
        exports_chain *ec = new exports_chain;
        this->p = ec;
        this->he.destroy = &chain_destroy;
        this->he.proceed = &chain_proceed;

        for(auto&& v: exports)
            ec->add(create_impl(v));
    }
}

exporter::exporter(exporter&& r)
{
    p = r.p;
    r.p = nullptr;
    he = r.he;
    r.he = hole_exporter();
}

void exporter::operator=(exporter&& r)
{
    p = r.p;
    r.p = nullptr;
    he = r.he;
    r.he = hole_exporter();
}

static const uint32_t register_log_message = register_exporter("log_messages", {&hole_no_init, &hole_no_destroy, &log_message});
static const uint32_t register_tyra = register_exporter("tyra", {&tyra_create, &tyra_destroy, &tyra_proceed});
static const uint32_t register_pipe = register_exporter("pipe", {&pipe_init, &pipe_destroy, &pipe_proceed});
static const uint32_t register_mmap = register_exporter("mmap_cp", {&mmap_init, &mmap_destroy, &mmap_proceed});
static const uint32_t register_null = register_exporter("/dev/null", {&hole_no_init, &hole_no_destroy, &hole_no_proceed});

