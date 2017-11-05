#include "log.hpp"
#include "time.hpp"

#include <fstream>

class simple_log
{
public:
    std::mutex mutex;
    uint32_t oparams;
    std::ofstream os;

    simple_log(const std::string& fname, uint32_t params) : oparams(params)
    {
        os.open(fname.c_str(), std::ofstream::out | std::ofstream::app);
        if(!os)
            throw std::runtime_error(es() % "open log file \"" % fname % "\" error");
    }
};


simple_log* sl = 0;

uint32_t& log_params()
{
    return sl->oparams;
}

log_init::log_init(const std::string& fname, uint32_t params)
{
    sl = new simple_log(fname, params);
}

log_init::~log_init()
{
    delete sl;
    sl = 0;
}

void log_set(simple_log* s)
{
    sl = s;
}

simple_log* log_get()
{
    return sl;
}

bool mlog::need_cout() const
{
    return params & cout || sl->oparams & cout;
}

mlog::mlog(uint32_t params) : s(sl->os.is_open() ? sl->os : std::cerr), params(params)
{
    sl->mutex.lock();

    (*this) << get_cur_ttime() << " ";
    if(params & mlog::warning)
        (*this) << "WARNING ";
    else if(params & mlog::error)
        (*this) << "ERROR ";
    else
    {
    }
}

mlog::~mlog()
{
    s << std::endl;
    if(need_cout())
        std::cout << std::endl;
    sl->mutex.unlock();
}

std::string itoa_hex(uint8_t c)
{
    std::string tmp = "  ";
    sprintf(&tmp[0],"%02X ",c);
    return std::move(tmp);
}

