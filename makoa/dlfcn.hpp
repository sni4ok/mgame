#include "../evie/mstring.hpp"
#include "../evie/smart_ptr.hpp"
#include "../evie/string.hpp"

#include <dlfcn.h>

static void shared_destroy(void* handle)
{
    if(handle)
        dlclose(handle);
}

typedef unique_ptr<void, shared_destroy> fcn_type;

template<typename func_t>
static pair<fcn_type, func_t*> load_lib(const mstring& lib, char_cit func)
{
    unique_ptr<void, shared_destroy> p(dlopen(lib.c_str(), RTLD_NOW));
    if(!p)
        throw_system_failure(es() % "load_lib, load error " % _str_holder(dlerror()));

    void* f = dlsym(p.get(), func);
    if(!f)
        throw_system_failure(es() % "load_lib, no " % _str_holder(func) % " in " % lib);

    return {move(p), (func_t*)f};
}

