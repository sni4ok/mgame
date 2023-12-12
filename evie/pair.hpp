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

template<typename t1, typename t2>
bool operator<(const pair<t1, t2>& l, const pair<t1, t2>& r)
{
    if(l.first == r.first)
        return l.second < r.second;
    return l.first < r.first;
}

template<typename t1, typename t2>
bool operator!=(const pair<t1, t2>& l, const pair<t1, t2>& r)
{
    return l.first != r.first || l.second != r.second;
}

template<typename t1, typename t2>
bool operator==(const pair<t1, t2>& l, const pair<t1, t2>& r)
{
    return l.first == r.first && l.second == r.second;
}

