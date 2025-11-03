#pragma once

#include "vector.hpp"
#include "mfile.hpp"
#include "string.hpp"
#include "algorithm.hpp"
#include "shared_ptr.hpp"

struct xml_element
{
    static constexpr str_holder all = str_holder();

    shared_ptr<mvector<char> > buf;
    str_holder data;

    char_cit begin() const
    {
        return data.begin();
    }
    char_cit end() const
    {
        return data.end();
    }
    static char_cit search_end(char_cit f, char_cit t, char_cit sf, char_cit st)
    {
        char_cit e = f;
        {
            char_cit i = f;
            for(; i != t; ++i)
            {
                if(*i == '<' || *i == '>')
                    break;
                else if(*i == '/')
                {
                    if(i != t && *(i + 1) == '>')
                        return i;
                }
            }
        }
    rep:
        e = search(e, t, sf, st);
        if(e != t && (*(e + (st - sf)) != '>' || *(e - 1) != '/' || *(e - 2) != '<')) {
            e += (st - sf);
            goto rep;
        }
        return e;
    }
    static void throw_error()
    {
        throw str_exception("tin_xml() parsing error");
    }
    static void skip_blanks(char_cit& i, char_cit e)
    {
        while(i != e && is_any_of("\r\n \t")(*i))
            ++i;
    }
    void skip_xml_tags()
    {
        char_cit i = data.begin(), e = data.end();
        for(;;)
        {
            if(*i != '<')
                throw_error();
            if(*(i + 1) != '?' && *(i + 1) != '!')
            {
                data = {i, e};
                return;
            }
            i = find(i, e, '>');
            if(i == e)
                throw_error();
            ++i;
            skip_blanks(i, e);
        }
    }
    xml_element first_child_impl(str_holder data, str_holder name, bool optional) const
    {
        char_cit i = data.begin();
        char_cit e = data.end();
    rep:
        i = find(i, e, '<');
        char_cit s = find_if(i, e, is_any_of(" >"));
        if(s != e)
        {
            ++i;
            str_holder tag(i, s);
            i += tag.size();
            if(tag == "!--")
            {
                i = find(i, data.end(), '>');
                goto rep;
            }
            else if(name == all || tag == name)
                e = search_end(i, data.end(), tag.begin(), tag.end());
            else
            {
                i = search_end(i, data.end(), tag.begin(), tag.end());
                goto rep;
            }
        }

        if(e == data.end())
        {
            if(optional)
                return {buf, str_holder()};
            throw mexception(es() % "tin_xml::first_child(), " % name % " not found");
        }
        return {buf, {i, e}};
    }
    xml_element first_child(str_holder name, bool optional = false) const
    {
        return first_child_impl(data, name, optional);
    }
    xml_element next_sibling(const xml_element& root, str_holder name, bool optional = false) const
    {
        char_cit i = end();
        char_cit e = root.end();
        while(i != e && *i != '>')
            ++i;
        if(i != e)
            ++i;
        return first_child_impl({i, e}, name, optional);
    }
    template<typename func>
    void for_each(str_holder name, func fun) const
    {
        xml_element e = first_child(name, true);
        while(!!e)
        {
            fun(e);
            e = e.next_sibling(*this, name, true);
        }
    }
    template<typename func>
    void for_each(func fun) const
    {
        return for_each(all, fun);
    }
    bool operator!() const
    {
        return data.empty();
    }
    str_holder value() const
    {
        char_cit i = find(data.begin(), data.end(), '>');
        char_cit e = find(i, data.end(), '<');
        if(e == data.end())
            throw str_exception("tin_xml::value(), not found");
        return {i + 1, e};
    }
    str_holder name() const
    {
        if(*data.begin() != ' ')
            throw_error();
        char_cit i = data.begin(), b = buf->begin();
        while(i != b && *i != '<')
            --i;
        if(*i != '<')
            throw_error();
        return {i + 1, data.begin()};
    }
    str_holder attribute(str_holder name, bool optional = false) const
    {
        char_cit i = data.begin() + 1;
        char_cit e = find(i, data.end(), '>');
    rep:
        i = search(i, e, name.begin(), name.end());
        if(i == e)
        {
            if(!optional)
                throw mexception(es() % "tin_xml::attribute(), " % name % " not found");
            else
                return str_holder();
        }
        if(*(i - 1) != ' ' || *(i + name.size()) != '=' || *(i + name.size() + 1) != '"')
        {
            i += name.size();
            goto rep;
        }
        i += name.size() + 2;
        return {i, find(i, e, '"')};
    }
    template<typename func>
    bool for_each_attribute(func fun) const
    {
        bool ret = false;
        char_cit i = data.begin() + 1, e = data.end();
        e = find(i, e, '>');
        for(;;)
        {
            skip_blanks(i, e);
            char_cit s = find(i, e, '=');
            if(s == e)
                return ret;
            str_holder name{i, s};
            i = find(s, e, '"');
            if(i == e)
                throw_error();
            ++i;
            s = find(i, e, '"');
            if(s == e)
                throw_error();
            fun(name, {i, s});
            ret = true;
            i = s + 1;
        }
    }

    template<typename type>
    type get_item(str_holder name, bool optional = false)
    {
        xml_element e = first_child(name, optional);
        if(!e)
            return type();
        else
            return lexical_cast<type>(e.value());
    }
    template<typename type>
    type get_attribute(str_holder name, bool optional = false)
    {
        str_holder a = attribute(name, optional);
        if(optional && a.empty())
            return type();
        else
            return lexical_cast<type>(a);
    }
};

inline xml_element parse_xml(mvector<char>&& data)
{
    if(data.size() < 1)
        throw str_exception("parse_xml() error");
    shared_ptr<mvector<char> > buf(new mvector<char>);
    *buf = move(data);
    xml_element e = {buf, {buf->begin(), buf->end()}};
    e.skip_xml_tags();
    return e;
}

inline xml_element load_and_parse_xml(char_cit fname)
{
    return parse_xml(read_file(fname));
}

