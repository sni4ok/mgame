#pragma once

#include "utils.hpp"
#include "config.hpp"

#include <string>
#include <memory>

#include <mysql.h>

namespace mysql
{
    struct dsn
    {
        std::string host;
        int port;
        std::string db_name;
        std::string user;
        std::string password;

        dsn() : port()
        {
        }
        void read(const std::string& buf)
        {
            host = get_config_param<std::string>(buf, "mysql_host");
            port = get_config_param<int>(buf, "mysql_port", true);
            db_name = get_config_param<std::string>(buf, "mysql_db_name", true);
            user = get_config_param<std::string>(buf, "mysql_user");
            password = get_config_param<std::string>(buf, "mysql_password", true);
        }
    };
    class connection
    {
        std::unique_ptr<MYSQL, decltype(&mysql_close)> sql;

    public:
        connection(const char *host, const char *user = 0, const char *passwd = 0, const char *db = 0, unsigned int port = 0)
            : sql(mysql_init(0), &mysql_close)
        {
            if(!sql)
                throw std::bad_alloc();

            if(!mysql_real_connect(sql.get(), host, user, passwd, db, port, NULL, 0))
                throw_error();
        }
        connection(const dsn& d) :
            connection(d.host.c_str(), d.user.c_str(), d.password.c_str(), d.db_name.c_str(), d.port)
        {
        }
        operator MYSQL*()
        {
            return sql.get();
        }
        void throw_error()
        {
            throw std::runtime_error(mysql_error(sql.get()));
        }
    };
}

