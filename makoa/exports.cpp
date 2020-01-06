/*
    shared library consumers for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "exports.hpp"
#include "types.hpp"

#include "tyra/tyra.hpp"

#include "evie/utils.hpp"
#include "evie/mlog.hpp"

#include <dlfcn.h>

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
                throw std::runtime_error("exports_chain max_size exceed");
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
    exporter shared_exporter(const std::string& module);
    struct exports_factory : noncopyable
    {
        uint32_t register_exporter(const std::string& module, hole_exporter he)
        {
            auto it = exporters.find(module);
            if(it != exporters.end())
                throw std::runtime_error(es() % "exports_factory() double registration for " % module);
            exporters[module] = he;
            return exporters.size();
        }
        hole_exporter get(const std::string& module)
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

        std::map<std::string, hole_exporter> exporters;
        std::list<exporter> dyn_exporters;
    };

    exports_factory efactory;
    
    void shared_destroy(void* handle)
    {
        if(handle)
            dlclose(handle);
    }
    exporter shared_exporter(const std::string& module)
    {
        std::string lib = "./lib" + module + ".so";
        exporter ret;
        ret.he.destroy = &shared_destroy;
        ret.p = dlopen(lib.c_str(), RTLD_LAZY);
        if(!ret.p)
            throw_system_failure(es() % "load library '" % lib % "' error");
        typedef void (create_hole)(hole_exporter* m, simple_log* sl);
        create_hole *hole = (create_hole*)dlsym(ret.p, "create_hole");
        if(!hole)
            throw_system_failure(es() % "not found create_hole() in '" % lib % "'");

        efactory.dyn_exporters.push_back(std::move(ret));
        exporter e;
        hole(&e.he, log_get());
        return e;
    }

    exporter create_impl(const std::string& m)
    {
        auto ib = m.begin(), ie = m.end(), it = std::find(ib, ie, ' ');
        std::string module, params;
        if(it == ie || it + 1 == ie)
            module = m;
        else {
            module = std::string(ib, it);
            params = std::string(it + 1, ie);
        }
        exporter ret;
        ret.he = efactory.get(module);
        ret.p = ret.he.init(params.c_str());
        return ret;
    }
}

uint32_t register_exporter(const std::string& module, hole_exporter he)
{
    return efactory.register_exporter(module, he);
}

exporter::exporter(const std::string& params)
{
    std::vector<std::string> exports = split(params, ';');
    if(params.empty())
        throw std::runtime_error("create_exporter() with empty params");
    if(exports.size() == 1)
        (*this) = std::move(create_impl(exports[0]));
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

