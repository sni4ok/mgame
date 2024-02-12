/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

template<typename f, typename s>
struct pair
{
    f first;
    s second;

    f begin() const
    {
        return first;
    }
    f end() const
    {
        return second;
    }
};

template<typename f, typename s>
bool operator<(const pair<f, s>& l, const pair<f, s>& r)
{
    if(l.first == r.first)
        return l.second < r.second;
    return l.first < r.first;
}

template<typename f, typename s>
bool operator!=(const pair<f, s>& l, const pair<f, s>& r)
{
    return l.first != r.first || l.second != r.second;
}

template<typename f, typename s>
bool operator==(const pair<f, s>& l, const pair<f, s>& r)
{
    return l.first == r.first && l.second == r.second;
}

template<typename stream, typename f, typename s>
stream& operator<<(stream& str, const pair<f, s>& p)
{
    str << p.first << ',' << p.second;
    return str;
}

