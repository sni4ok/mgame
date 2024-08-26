/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "algorithm.hpp"
#include "mstring.hpp"
#include "string.hpp"

template<typename ... types>
struct tuple;

template<>
struct tuple<>
{
    static constexpr size_t size() {
        return 0;
    }
    bool operator==(const tuple&) const
    {
        return true;
    }
    bool operator!=(const tuple&) const
    {
        return false;
    }
    bool operator<(const tuple&) const
    {
        return false;
    }
};

template<typename type>
struct tuple<type>
{
    typedef type element_type;
    typedef void values_type;

    type v;

    template<size_t i>
    type& get() requires(!i) {
        return v;
    }
    template<size_t i>
    const type& get() const requires(!i) {
        return v;
    }
    static constexpr size_t size() {
        return 1;
    }
    template<typename tuple2>
    auto add_front(const tuple2& t2) const
    {
        return tuple<typename tuple2::element_type, type>(t2.v, {v});
    }
    bool operator==(const tuple& r) const
    {
        return v == r.v;
    }
    bool operator!=(const tuple& r) const
    {
        return v != r.v;
    }
    bool operator<(const tuple& r) const
    {
        return v < r.v;
    }
};

template<typename type, typename ... types>
struct tuple<type, types...>
{
    typedef type element_type;
    typedef tuple<types...> values_type;

    type v;
    tuple<types...> values;

    template<size_t i>
    auto& get() {
        if constexpr(!i)
            return v;
        else
            return values.template get<i - 1>();
    }
    template<size_t i>
    const auto& get() const {
        if constexpr(!i)
            return v;
        else
            return values.template get<i - 1>();
    }
    static constexpr size_t size() {
        return 1 + tuple<types...>::size();
    }
    template<typename tuple2>
    auto add_front(const tuple2& t2) const
    {
        return tuple<typename tuple2::element_type, type, types...>(t2.v, {v, values});
    }
    bool operator==(const tuple& r) const
    {
        return v == r.v && values == r.values;
    }
    bool operator!=(const tuple& r) const
    {
        return v != r.v || values != r.values;
    }
    bool operator<(const tuple& r) const
    {
        if(v < r.v)
            return true;
        return values < r.values;
    }
};

template<typename t>
concept __have_size = is_class_v<t> && requires(t* v)
{
    v->size();
};

template<typename t>
requires(__have_size<t>)
struct tuple_size
{
    static constexpr size_t value = t::size();
};

template<typename t>
inline constexpr size_t tuple_size_v = tuple_size<t>::value;

template<size_t i, typename t>
struct tuple_element
{
    typedef conditional_t<!i, typename t::element_type,
        typename tuple_element<i - 1, typename t::values_type>::type
    > type;
};

template<size_t i>
struct tuple_element<i, void>
{
    typedef void type;
};

template<size_t i, typename t>
using tuple_element_t = typename tuple_element<i, t>::type;

template<size_t i, typename ... types>
auto& get(tuple<types...>& t)
{
    return t.template get<i>();
}

template<size_t i, typename ... types>
const auto& get(const tuple<types...>& t)
{
    return t.template get<i>();
}

template<typename ... types>
auto make_tuple(types ... args)
{
    return tuple<types...>({args...});
}

template<typename tuple1, typename tuple2>
auto tuple_cat(const tuple1& t1, const tuple2& t2)
{
    if constexpr(tuple_size_v<tuple2> == 0)
        return t1;
    else {
        static const size_t sz = tuple_size_v<tuple1>;
        if constexpr(sz == 0)
            return t2;
        else if constexpr(sz == 1)
            return t2.add_front(t1);
        else if constexpr(sz == 2) {
            auto r = t2.add_front(t1.values);
            return r.add_front(t1);
        }
        else if constexpr(sz == 3) {
            auto r = t2.add_front(t1.values.values);
            auto w = r.add_front(t1.values);
            return w.add_front(t1);
        }
        else
            static_assert(false);
    }
}

template<typename func, typename tuple>
auto apply(func& f, tuple& t)
{
    static const size_t sz = tuple_size_v<tuple>;
    if constexpr(is_member_function_pointer_v<func>)
    {
        if constexpr(sz == 1)
            return (get<0>(t)->*f)();
        else if constexpr(sz == 2)
            return (get<0>(t)->*f)(get<1>(t));
        else if constexpr(sz == 3)
            return (get<0>(t)->*f)(get<1>(t), get<2>(t));
        else if constexpr(sz == 4)
            return (get<0>(t)->*f)(get<1>(t), get<2>(t), get<3>(t));
        else
            static_assert(false);
    }
    else
    {
        if constexpr(sz == 0)
            return f();
        else if constexpr(sz == 1)
            return f(get<0>(t));
        else if constexpr(sz == 2)
            return f(get<0>(t), get<1>(t));
        else if constexpr(sz == 3)
            return f(get<0>(t), get<1>(t), get<2>(t));
        else if constexpr(sz == 4)
            return f(get<0>(t), get<1>(t), get<2>(t), get<3>(t));
        else if constexpr(sz == 5)
            return f(get<0>(t), get<1>(t), get<2>(t), get<3>(t), get<4>(t));
        else
            static_assert(false);
    }
    assert(false);
}

template<size_t sz, typename stream, typename ... types>
void write_tuple_elem(stream& str, const tuple<types...>& p)
{
    if constexpr(sizeof...(types))
    {
        if constexpr(sz)
            str << ",";
        str << get<sz>(p);
        if constexpr(sizeof...(types) - sz - 1)
            write_tuple_elem<sz + 1>(str, p);
    }
}

template<typename tuple>
mstring tuple_to_string(const tuple& v)
{
    mstring ret;
    ret.resize(256);
    buf_stream str(&ret[0], &ret[0] + ret.size());
    write_tuple_elem<0>(str, v);
    ret.resize(str.size());
    return ret;
}

template<typename value_type>
struct type_value
{
    typedef value_type type;
};

template<typename tuple, size_t sz = 0, template<typename> typename type = type_value, typename func>
void tuple_for_each(func f)
{
    if constexpr(tuple_size_v<tuple>)
    {
        f(type<tuple_element_t<sz, tuple> >());
        if constexpr(tuple_size_v<tuple> - sz - 1)
            tuple_for_each<tuple, sz + 1, type, func>(f);
    }
}

template<typename tuple, size_t sz = 0, template<typename> typename type = type_value, typename func>
bool tuple_for_each_one(func f)
{
    if constexpr(!(tuple_size_v<tuple>))
        return false;
    if(f(type<tuple_element_t<sz, tuple> >()))
        return true;
    if constexpr(tuple_size_v<tuple> - sz - 1)
        return tuple_for_each_one<tuple, sz + 1, type, func>(f);
    else
        return false;
}

template<typename tuple, typename str, size_t sz = 0>
void tuple_from_strings(const mvector<str>& params, tuple& p)
{
    if constexpr(tuple_size_v<tuple>)
    {
        get<sz>(p) = lexical_cast<tuple_element_t<sz, tuple> >(params[sz]);
        if constexpr(tuple_size_v<tuple> - sz - 1)
            tuple_from_strings<tuple, str, sz + 1>(params, p);
    }
}

template<int>
auto read_csv_impl(char_cit, char_cit, char)
{
    return tuple<>();
}

template<int idx, typename type, typename ... types>
auto read_csv_impl(char_cit& i, char_cit e, char sep)
{
    char_cit te, t;
    if((e - i) >= 2 && *i == '\"') {
        ++i;
        te = find(i, e, '\"');
        if(te == e)
            throw mexception(es() % "read_csv failure: " % str_holder(i, e));
        t = te + 1;
    }
    else {
        te = find(i, e, sep);
        t = te;
    }
    type v = lexical_cast<type>(i, te);
    i = t;
    if(i != e)
        ++i;
    auto ret = tuple_cat(make_tuple(v), read_csv_impl<idx + 1, types...>(i, e, sep));
    if(i != e)
        throw mexception(es() % "read_csv not complete: " % str_holder(i, e));
    return ret;
}

template<typename ... types, typename func>
void read_csv(func f, char_cit i, char_cit e, char sep = ',')
{
    while(i != e)
    {
        char_cit t = find(i, e, endl);
        if(t == e)
            throw str_exception("read_csv no endl found");
        if(*i == '#') {
            i = t + 1;
            continue;
        }
        auto v = read_csv_impl<0, types...>(i, t, sep);
        f(v);
        i = t + 1;
    }
}

