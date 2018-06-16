/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "evie/utils.hpp"

#include <curl/curl.h>

struct curl_err : noncopyable
{
    curl_err()
    {
    }
    curl_err& operator=(CURLcode ret)
    {
        if(ret != CURLE_OK)
            throw std::runtime_error(es() % "curl error: " % curl_easy_strerror(ret));
        return *this;
    }
    curl_err& operator&(CURLcode ret)
    {
        if(ret != CURLE_OK)
            throw std::runtime_error(es() % "curl error: " % curl_easy_strerror(ret));
        return *this;
    }
};

struct curl : noncopyable
{
    CURL *c;
public:
    curl()
    {
        c = curl_easy_init();
    }
    operator CURL*()
    {
        return c;
    }
    ~curl()
    {
        curl_easy_cleanup(c);
    }
};

