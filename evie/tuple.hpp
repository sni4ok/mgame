/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "print.hpp"
#include "pair.hpp"
#include "type_traits.hpp"

template<typename ... types>
struct tuple;

template<>
struct tuple<>
{
    static constexpr size_t tuple_size()
    {
        return 0;
    }
    bool operator==(const tuple&) const
    {
        return true;
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
    type& get() requires(!i)
    {
        return v;
    }
    template<size_t i>
    const type& get() const requires(!i)
    {
        return v;
    }
    static constexpr size_t tuple_size()
    {
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
    auto& get()
    {
        if constexpr(!i)
            return v;
        else
            return values.template get<i - 1>();
    }
    template<size_t i>
    const auto& get() const
    {
        return const_cast<tuple<type, types...>* >(this)->template get<i>();
    }
    static constexpr size_t tuple_size()
    {
        return 1 + tuple<types...>::tuple_size();
    }
    template<typename tuple2>
    auto add_front(const tuple2& t2) const
    {
        return tuple<typename tuple2::element_type, type, types...>(t2.v, {v, values});
    }
    bool operator==(const tuple& r) const
    {
        return (v == r.v) && (values == r.values);
    }
    bool operator<(const tuple& r) const
    {
        if(v < r.v)
            return true;
        return values < r.values;
    }
};

template<typename t>
concept __have_tuple_size = __is_class(t) && requires(t* v)
{
    v->tuple_size();
};

template<typename t>
requires(__have_tuple_size<t>)
struct tuple_size
{
    static const size_t value = t::tuple_size();
};

template<typename t>
static const size_t tuple_size_v = tuple_size<t>::value;

template<size_t i, typename t>
struct tuple_element
{
    typedef conditional_t<!i, typename t::element_type,
        typename tuple_element<i - 1, typename t::values_type>::type
    > type;
};

template<size_t i, typename t>
struct tuple_element<i, const t>
{
    typedef const typename tuple_element<i, t>::type type;
};

template<size_t i, typename f, typename s>
struct tuple_element<i, pair<f, s> >
{
    typedef conditional_t<!i, f, s> type;
    static_assert(i < 2);
};

template<size_t i>
struct tuple_element<i, void>
{
    typedef void type;
};

namespace std
{
    template<size_t i, typename t>
    struct tuple_element : :: tuple_element<i, t>
    {
    };

    template<typename t>
    struct tuple_size;

    template<typename t>
    requires(__have_tuple_size<t>)
    struct tuple_size<t> : ::tuple_size<t>
    {
    };
}

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
    else
    {
        static const size_t sz = tuple_size_v<tuple1>;
        if constexpr(sz == 0)
            return t2;
        else if constexpr(sz == 1)
            return t2.add_front(t1);
        else if constexpr(sz == 2)
        {
            auto r = t2.add_front(t1.values);
            return r.add_front(t1);
        }
        else if constexpr(sz == 3)
        {
            auto r = t2.add_front(t1.values.values);
            auto w = r.add_front(t1.values);
            return w.add_front(t1);
        }
        else
            static_assert(false && sz);
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
            static_assert(false && (sz < 1 || sz > 4));
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
            static_assert(false && (sz < 1 || sz > 5));
    }
    ASSERT(false);
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

template<typename value_type>
struct type_value
{
    typedef value_type type;
};

template<typename tuple, template<typename> typename type = type_value, typename func, size_t sz = 0>
void tuple_for_each(func f)
{
    if constexpr(tuple_size_v<tuple>)
    {
        f(type<tuple_element_t<sz, tuple> >());
        if constexpr(tuple_size_v<tuple> - sz - 1)
            tuple_for_each<tuple, type, func, sz + 1>(f);
    }
}

template<typename tuple, typename func, size_t sz = 0>
void tuple_for_each(const tuple& t, func f)
{
    if constexpr(tuple_size_v<tuple>)
    {
        f(get<sz>(t));
        if constexpr(tuple_size_v<tuple> - sz - 1)
            tuple_for_each<tuple, func, sz + 1>(t, f);
    }
}

template<typename tuple, template<typename> typename type = type_value, typename func, size_t sz = 0>
bool tuple_for_each_one(func f)
{
    if constexpr(!(tuple_size_v<tuple>))
        return false;
    if(f(type<tuple_element_t<sz, tuple> >()))
        return true;
    if constexpr(tuple_size_v<tuple> - sz - 1)
        return tuple_for_each_one<tuple, type, func, sz + 1>(f);
    else
        return false;
}

template<typename tuple, typename vec, size_t sz = 0>
void tuple_from_strings(const vec& params, tuple& p)
{
    if constexpr(tuple_size_v<tuple>)
    {
        get<sz>(p) = lexical_cast<tuple_element_t<sz, tuple> >(params.at(sz));
        if constexpr(tuple_size_v<tuple> - sz - 1)
            tuple_from_strings<tuple, vec, sz + 1>(params, p);
    }
}

template<char s, typename func, typename tuple>
struct print_tuple_t
{
    func f;
    const tuple& t;
};

template<size_t sz, typename stream, char sep, typename func, typename tuple>
void print_tuple_impl(stream& s, const print_tuple_t<sep, func, tuple>& p)
{
    if constexpr(sz)
        s << sep;
    p.f(s, get<sz>(p.t));
    if constexpr(tuple_size_v<tuple> - sz - 1)
        print_tuple_impl<sz + 1>(s, p);
}

template<typename stream, char sep, typename func, typename tuple>
stream& operator<<(stream& s, const print_tuple_t<sep, func, tuple>& p)
{
    if constexpr(tuple_size_v<tuple>)
        print_tuple_impl<0>(s, p);
    return s;
}

template<char sep = ',', typename func = print_default, typename tuple>
print_tuple_t<sep, func, tuple> print_tuple(const tuple& t, func f = func())
{
    return {f, t};
}

