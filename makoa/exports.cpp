/*
    shared library consumers for makoa server
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "exports.hpp"

#include "evie/utils.hpp"

#include <dlfcn.h>

struct dl_holder
{
    void* handle;
    dl_holder() : handle()
    {
    }
    ~dl_holder()
    {
        if(handle)
            dlclose(handle);
    }
};

struct exports::impl : dl_holder
{
    void* p;
    hole_exporter m;
    impl(const std::string& module, void (*&proceed)(const message& m), const std::string& params) : p()
    {
        std::string lib = "./lib" + module + ".so";
        handle = dlopen(lib.c_str(), RTLD_LAZY);
        if(!handle)
            throw_system_failure(es() % "load library '" % lib % "' error");
        typedef void (create_hole)(hole_exporter* m, simple_log* sl);
        create_hole *hole = (create_hole*)dlsym(handle, "create_hole");
        if(!hole)
            throw_system_failure(es() % "not found create_hole() in '" % lib % "'");
        hole(&m, log_get());
        p = m.init(params);
        proceed = m.proceed;
    }
    ~impl()
    {
        m.destroy(p);
    }
};

exports::exports(const std::string& module)
{
    pimpl = std::make_unique<impl>(module, proceed, std::string());
}

exports::exports(const std::string& module, const std::string& params)
{
    pimpl = std::make_unique<impl>(module, proceed, params);
}

exports::~exports()
{
}

