/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "securities.hpp"

#include "evie/utils.hpp"
#include "evie/log.hpp"


struct securities::impl : stack_singleton<securities::impl>
{
    impl() {
    }
    uint32_t id(const message_instr& is) {
        crc32 crc(0);
        crc.process_bytes((const char*)&is, sizeof(message_instr) - 12);
        return crc.checksum();
    }
};

securities::securities()
{
    pimpl = std::make_unique<securities::impl>();
}

securities::~securities()
{
}

uint32_t get_security_id(const message_instr& is, bool check_equal)
{
    uint32_t security_id = securities::impl::instance().id(is);
    if(check_equal && security_id != is.security_id)
        throw std::runtime_error(es() % "get_security_id() not equals crc, in: " % is.security_id % ", calculated: " % security_id);
    return security_id;
}

