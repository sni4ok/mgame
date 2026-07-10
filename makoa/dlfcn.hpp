#pragma once

#include "../evie/mstring.hpp"
#include "../evie/unique_ptr.hpp"
#include "../evie/string.hpp"

#include <dlfcn.h>

inline void lib_free(void* handle)
{
    if(handle)
        dlclose(handle);
}

typedef unique_ptr<void, lib_free> fcn_type;

template<typename func_t>
pair<fcn_type, func_t*> lib_load(const mstring& lib, char_cit func)
{
    unique_ptr<void, lib_free> p(dlopen(lib.c_str(), RTLD_NOW));
    if(!p)
        throw_system_failure(es() % "lib_load, load error " % _str_holder(dlerror()));

    void* f = dlsym(p.get(), func);
    if(!f)
        throw_system_failure(es() % "load_load, no " % _str_holder(func) % " in " % lib);

    return {move(p), (func_t*)f};
}

