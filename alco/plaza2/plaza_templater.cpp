/*
    This file contains script for generating c++ types from plaza2 ini schemes

    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "../../evie/utils.hpp"
#include "../../evie/mfile.hpp"
#include "../../evie/config.hpp"
#include "../../evie/tuple.hpp"

#include <map>
#include <set>
#include <iostream>

#include <dirent.h>

typedef buf_stream_fixed<1024 * 1024> b_stream;

typedef pair<mstring, mstring> namespace_type;

struct stream_file : b_stream
{
    mstring fname;

    stream_file(const mstring& out_file) : fname(out_file) {
        write_file(fname.c_str(), begin(), size(), true);
    }
    ~stream_file() {
        write_file(fname.c_str(), begin(), size(), true);
    }
};

struct source_file : stream_file
{
    source_file(const mstring& out_file) : stream_file(out_file) {
        (*this) << "//autogenerated by plaza_templater\n\n"
            << "#pragma once\n"
            << "#pragma pack(push, 4)\n\n";
    }
    ~source_file() {
        (*this) << "#pragma pack(pop)\n\n";
    }
};

struct context
{
    namespace_type cur_namespace, saved_namespace;
    mstring table_name;
    std::map<namespace_type, std::map<mstring, mstring> > saved_structures;
    source_file sf;
    stream_file* fi;

    context(const mstring& out_file) : sf(out_file), fi()
    {
    }
    virtual bool need_save() {
        return true;
    }
    virtual ~context() {
    }
};

uint32_t cg_decimal_size(uint32_t m, uint32_t e) {
    return 2 + (m >> 1) + ((m | e) & 1); 
}

template<typename t>
struct optional
{
    bool set;
    t val;
    optional() : set() {
    }
    optional(const t& v) : set(true), val(v) {
    }
    optional& operator=(const optional& v) {
        val = v.val;
        set = v.set;
        return *this;
    }
    operator bool () const {
        return set;
    }
    const t& operator*() const {
        if(!set)
            throw str_exception("optional::operator* value not set");
        return val;
    }
};

struct skelet
{
    optional<uint32_t> msgid;
    optional<bool> request, reply;
    mvector<uint32_t> replies;
    mvector<tuple<mstring, mstring, optional<mstring> > > types;
    uint32_t structure_size;
    bool have_alignment;
    mstring nullable; //WTF IS THIS ?????
    void add_size(uint32_t sz, uint32_t granularity) {
        granularity = min(uint32_t(4), granularity);
        uint32_t st_from = structure_size % granularity;
        if(st_from)
            structure_size += (granularity - st_from);
        structure_size += sz;
    }
    std::set<str_holder> c_types;
    void close_structure_size() {
        uint32_t st_from = structure_size % 4;
        if(st_from && c_types.size() > 1)
            structure_size += (4 - st_from);
    }
    skelet() : structure_size(), have_alignment() {
    }
    void set_nullable(const mstring& data) {
        nullable = data;
    }
    void clear() {
        msgid = optional<uint32_t>();
        request = optional<bool>();
        reply = optional<bool>();
        replies.clear();
        types.clear();
        c_types.clear();
        structure_size = 0;
    }
    mstring tmp_str;
    str_holder convert_type(const mstring& ini_type) {
        if(ini_type.empty())
            throw mexception("empty ini_type!");
        if(ini_type == "i8") {
            add_size(8, 8);
            c_types.insert("int64_t");
            return "int64_t";
        }
        if(ini_type == "u8") {
            add_size(8, 8);
            c_types.insert("uint64_t");
            return "uint64_t";
        }
        if(ini_type == "i4") {
            add_size(4, 4);
            c_types.insert("int32_t");
            return "int32_t";
        }
        if(ini_type == "i2") {
            add_size(2, 2);
            c_types.insert("int16_t");
            return "int16_t";
        }
        if(ini_type == "i1") {
            add_size(1, 1);
            c_types.insert("int8_t");
            return "int8_t";
        }
        if(ini_type == "u1") {
            add_size(1, 1);
            c_types.insert("uint8_t");
            return "uint8_t";
        }
        if(ini_type == "f") {
            add_size(8, 8);
            c_types.insert("double");
            return "double";
        }
        if(ini_type == "t") {
            add_size(10, 2);
            c_types.insert("uint8_t");
            c_types.insert("uint16_t");
            return "cg_time_t";
        }
        if(ini_type[0] == 'c') {
            mstring v(ini_type.begin() + 1, ini_type.end());
            uint32_t vs = lexical_cast<uint32_t>(v);
            add_size(vs + 1, 1);
            tmp_str = mstring("cg_string<") + v + ">";
            c_types.insert("char");
            return tmp_str.str();
        }
        if(ini_type[0] == 'd') {
            mstring::const_iterator p = find(ini_type.begin() + 1, ini_type.end(), '.');
            mstring ms = mstring(ini_type.begin() + 1, p);
            uint32_t m = lexical_cast<uint32_t>(ms);
            mstring es = mstring(p + 1, ini_type.end());
            uint32_t e = lexical_cast<uint32_t>(mstring(p + 1, ini_type.end()));
            add_size(cg_decimal_size(m, e), 1);
            tmp_str = mstring("cg_decimal<") + ms + "," + es + ">";
            c_types.insert("char");
            return tmp_str.str();
        }
        throw mexception(es() % "unknown ini_type: " % ini_type);
    }
    void add_field(const mstring& data) {
        mvector<str_holder> tmp = split(data.str(), ',');
        if(tmp.size() != 2 && tmp.size() != 4)
            throw mexception(es() % "bad add_field: " % data);
        if(tmp.size() == 4 && tmp[2].size)
            cout() << data << endl;
        tuple<mstring, mstring, optional<mstring> > v;
        get<0>(v) = tmp[0];
        get<1>(v) = convert_type(tmp[1]);
        if(tmp.size() == 4)
            get<2>(v) = mstring(tmp[3]);
        types.push_back(v);
    }
    void set_replies(const mstring& data) {
        if(data.empty())
            return;
        if(!replies.empty())
            throw mexception("replies already exists");
        for(const auto& s: split(data.str(), ','))
            replies.push_back(lexical_cast<uint32_t>(s));
    }
    template<typename value>
    static void write_body_value(b_stream& of, str_holder type, const value& v) {
        of << "        " << type << " " << v << ";" << endl;
    }
    void clear_and_close_namespace(context& ctx, bool end) {
        clear();
        if(end) {
            if(!ctx.saved_namespace.first.empty())
                ctx.sf << "}";
            if(!ctx.saved_namespace.second.empty())
                ctx.sf << "}" << endl;
            
            ctx.saved_namespace = namespace_type();
        }
    }
    void commit(context& ctx, bool end, char_cit i_f, char_cit i_t) {
        if(types.empty() || !ctx.need_save()) {
            clear_and_close_namespace(ctx, end);
            return;
        }
        if(ctx.fi)
            ctx.fi->write(&(*i_f), i_t - i_f);
        if(ctx.saved_namespace != ctx.cur_namespace) {
            if(ctx.saved_namespace.first == ctx.cur_namespace.first) {
                if(!ctx.saved_namespace.second.empty())
                    ctx.sf << "}" << endl;
                ctx.sf << "namespace " << ctx.cur_namespace.second << endl << "{" << endl;
            }
            else{
                if(!ctx.saved_namespace.first.empty())
                    ctx.sf << "}";
                if(!ctx.saved_namespace.second.empty())
                    ctx.sf << "}" << endl;
                ctx.sf << "namespace " << ctx.cur_namespace.first << endl << "{ " << endl << "namespace "
                    << ctx.cur_namespace.second << endl << "{" << endl;
            }
            ctx.saved_namespace = ctx.cur_namespace;
        }
        b_stream str;
        str << "    struct " << ctx.table_name << endl << "    {" << endl;
        write_body_value(str, "static constexpr uint32_t plaza_size =", structure_size);
        if(msgid)
            write_body_value(str, "static constexpr uint32_t msgid =", *msgid);
        if(request)
            write_body_value(str, "static constexpr bool request =", *request);
        if(reply)
            write_body_value(str, "static constexpr bool reply =", *reply);
        if(!replies.empty()) {
            b_stream type;
            type << "//typedef mpl::vector_c<uint32_t";
            for(auto s: replies)
                type << ", " << s;
            type << ">";
            write_body_value(str, type.str(), "replies");
        }
        if(!nullable.empty())
            write_body_value(str, "//nullable =", nullable);
        for(const auto& v: types) {
            write_body_value(str, get<1>(v).str(), get<0>(v));
        }
        if(reply) {
            str << "        void print(mlog& ml) {" << endl <<  "            ml << \"" << ctx.table_name << "\"";
            for(const auto& v: types) {
                str << " << \", " <<  get<0>(v) << ": \" << " << get<0>(v);
            }
            str << ";" << endl << "        }" << endl;
        }
        {

            str << "        void print_brief() {" << endl <<  "            mlog() << \"" << ctx.table_name << "|\"";
            uint32_t cur_i = 0;
            for(const auto& v: types) {
                if(cur_i && !(cur_i % 5)) {
                    str << endl << "              ";
                }
                str << " << " << get<0>(v) << " << \"|\"";
                ++cur_i;
            }
            str << ";" << endl << "        }" << endl;
        }
        {
            bool have_init = false;
            for(const auto& v: types) {
                if(get<2>(v)) {
                    have_init = true;
                    break;
                }
            }
            if(have_init) {
                //write constructor
                str << "        " << ctx.table_name << "() :";
                uint32_t cur_i = 0;
                for(const auto& v: types) {
                    if(get<2>(v)) {
                        if(cur_i)
                            str << ", ";
                        if(!(cur_i % 5)) {
                            str << endl << "          ";
                        }
                        str << get<0>(v) << "(";
                        str << *get<2>(v);
                        str <<  ")";
                        ++cur_i;
                    }
                }
                str << endl << "        {" << endl << "        }" << endl;
            }
        }
        close_structure_size();
        str << "    };" << endl;
        str << "    static_assert(sizeof(" << ctx.table_name << ") == " << structure_size << ", \"" << ctx.table_name << "\");" << endl;
        std::map<mstring, mstring>& cur = ctx.saved_structures[ctx.cur_namespace];
        mstring cur_str(str.str());
        auto it = cur.find(ctx.table_name);
        if(it == cur.end()) {
            bool found = false;
            for(const auto& i : ctx.saved_structures) {
                auto ii = i.second.find(ctx.table_name);
                if(ii != i.second.end() && ii->second == cur_str) {
                    found = true;
                    ctx.sf << "    using " << i.first.first << "::" << i.first.second << "::" << ctx.table_name << ";" << endl;
                    break;
                }
            }
            if(!found) {
                ctx.sf.write(cur_str.c_str(), cur_str.size());
                cur[ctx.table_name] = cur_str;
            }
        }
        else{
            if(it->second != cur_str) {
                std::cerr << "bad redinfication of struct: " << ctx.cur_namespace.first << "::"
                    << ctx.cur_namespace.second << "::" << ctx.table_name << endl;
                ctx.sf << "    //struct " << ctx.table_name << " badly defined early" << endl;
            }
            else
                ctx.sf << "    //struct " << ctx.table_name << " already defined early" << endl;
        }
        clear_and_close_namespace(ctx, end);
    }
};

void check_no_endl(const mstring& v)
{
    auto it = find(v.begin(), v.end(), '\n');
    if(it != v.end())
        throw mexception(es() % "string with endl, parse error: " % v);
}

void parse_file(context& ctx, const mvector<char>& data)
{
    mvector<char>::const_iterator it = data.begin(), it_e = data.end(), it_tmp, ini_from = it;
    skelet sk;
    //const mstring stream_name = "\x20\xCF\xEE\xF2\xEE\xEA\x20"; //" Potok "
    const str_holder stream_name = "\x20\xD0\x9F\xD0\xBE\xD1\x82\xD0\xBE\xD0\xBA\x20"; //" Potok "
    while(it != it_e) {
        while(it != it_e && (*it == '\r' || *it == '\n'))
            ++it;
        if(it != it_e && *it == ';') {
            ++it;
            auto i = stream_name.begin(), ie = stream_name.end();
            while(it != it_e && i != ie) {
                if(*it != *i)
                    break;
                ++it;
                ++i;
            }
            if(i == ie) {
                sk.commit(ctx, false, ini_from, it);
                it_tmp = find(it, it_e, ' ');
                ctx.cur_namespace.first = mstring(it, it_tmp);
            }

            while(it != it_e && *it != '\n')
                ++it;
            continue;
        }
        if(it == it_e)
            break;
        if(it != it_e && *it == '[') {
            it_tmp = find(++it, it_e, ':');
            if(it_tmp == it_e) break;
            sk.commit(ctx, false, ini_from, it - 1);
            ini_from = it - 1;
            if(mstring(it, it_tmp) == "table") {
                it = it_tmp + 1;
                it_tmp = find(it, it_e, ':');
                if(it_tmp == it_e)
                    throw mexception("parsing error 1");
                ctx.cur_namespace.second = mstring(it, it_tmp);
                check_no_endl(ctx.cur_namespace.second);
                it = it_tmp + 1;
                it_tmp = find(it, it_e, ']');
                if(it_tmp == it_e)
                    throw mexception("parsing error 2");
                ctx.table_name = mstring(it, it_tmp);
                check_no_endl(ctx.table_name);
            }
            it = it_tmp + 1;
            it = find_if(it, it_e, [](char c) {return c == '\r' || c == '\n';});
            continue;
        }
        {
            it_tmp = find(it, it_e, '=');
            if(it_tmp == it_e)
                throw mexception("parsing error 3");
            mstring type(it, it_tmp);
            while(!type.empty() && *type.rbegin() == ' ')
                type.pop_back();
            check_no_endl(type);
            it = it_tmp + 1;
            if(it == it_e)
                throw mexception("parsing error 4");
            it_tmp = find_if(it, it_e, [](char c) {return c == '\r' || c == '\n';});
            mstring data(it, it_tmp);
            check_no_endl(data);
            while(!data.empty() && *data.begin() == ' ')
                data = mstring(data.begin() + 1, data.end());;

            if(type == "table") {
            }
            else if(type == "index") {
            }
            else if(type == "field") {
                sk.add_field(data);
            }
            else if(type == "LocalTimeField") {
            }
            else if(type == "msgid") {
                sk.msgid = lexical_cast<uint32_t>(data);
            }
            else if(type == "request") {
                sk.request = lexical_cast<bool>(data);
            }
            else if(type == "reply") {
                sk.reply = lexical_cast<bool>(data);
            }
            else if(type == "replies") {
                sk.set_replies(data);
            }
            else if(type == "nullable") {
                sk.set_nullable(data);
            }
            else if(type == "hint") {
                //what is this?
            }
            else if(type == "isSyncService") {
            }
            else if(type == "REC_TIME_FIELD") {
            }
            else
                throw mexception(es() % "unknown type: " % type);

            if(it_tmp == it_e)
                break;
            it = it_tmp + 1;
        }
    }
    sk.commit(ctx, true, ini_from, it);
}

void parse_file(context& ctx, const mvector<char>& data, const mstring& fname)
{
    try {
        parse_file(ctx, data);
    }
    catch(exception& e) {
        throw mexception(es() % fname % ": " % _str_holder(e.what()));
    }
}

mvector<mstring> list_folder(const mstring& ini_folder)
{
    mvector<mstring> files;

    auto close_dir = [](DIR* d) {
        closedir(d);
    };

    unique_ptr<DIR, close_dir> dp(opendir(ini_folder.c_str()));
    if(!dp)
        throw mexception(es() % "opening directory \'" % ini_folder % "\' error");
    dirent *dirp;

    while((dirp = readdir(dp.get())) != NULL) {
        mstring fname(dirp->d_name);
        if(dirp->d_type == DT_REG && fname.size() > 4 && *fname.rbegin() == 'i' && *(fname.rbegin() + 1) == 'n'
         && *(fname.rbegin() + 2) == 'i' && *(fname.rbegin() + 3) == '.')
            files.push_back(fname);
    }
    return files;
}

void parse_all(const mstring& ini_folder, const mstring& out_file)
{
    mvector<mstring> files = list_folder(ini_folder);
    context ctx(out_file);
    for(auto&& fname: files) {
        ctx.cur_namespace = namespace_type();
        cout() << "file: " << fname << endl;
        ctx.sf << "// file: " << fname << endl;
        parse_file(ctx, read_file((ini_folder + "/" + fname).c_str()), fname);
    }
    std::map<mstring, namespace_type> unique_tables;
    {
        for(const auto& i : ctx.saved_structures) {
            for(const auto& ii : i.second) {
                auto it = unique_tables.find(ii.first);
                if(it == unique_tables.end())
                    unique_tables[ii.first] = i.first;
                else
                    it->second = namespace_type();
            }
        }
    }
    for(const auto& v: unique_tables) {
        if(v.second != namespace_type())
            ctx.sf << "using " << v.second.first << "::" << v.second.second << "::" << v.first << ";" << endl;
    }
    cout() << out_file << " successfully written" << endl;
}

struct source : context
{
    mstring name;
    namespace_type ns;
    std::set<mstring> tables;

    stream_file ini_out;
    b_stream tail;

    source(const mstring& name)
        try : context(name + ".inl"), name(name), ini_out(name + ".ini")
    {
        fi = &ini_out;
    }
    catch(exception& e) {
        throw_system_failure(es() % "open " % name % ": " % _str_holder(e.what()));
    }
    void write_head(const mstring& fname) {
        sf << "const char* cfg_cli_" << fname << " = \"p2repl://" << ns.first
            << ";scheme=|FILE|" << name << ".ini|" << ns.second << "\";" << endl << endl;

        ini_out << "[dbscheme:" << ns.second << "]\r\n";
        for(auto&& s: tables)
            ini_out << "table=" << s << "\r\n";
        ini_out << "\r\n";
    }
    void write_cpp_tail() {
        sf << tail.str();
    }
    bool need_save() {
        if(ns == cur_namespace) {
            std::set<mstring>::iterator it = tables.find(table_name);
            if(it != tables.end()) {
                tables.erase(it);
                tail << "using " << ns.first << "::" << ns.second << "::" << table_name << ";" << endl;
                return true;
            }
        }
        return false;
    }
    void check_fill() {
        if(!tables.empty())
            throw mexception(es() % "source \'" % name % "\' not filled");
    }
};

void proceed_selected(const mstring& ini_folder, const mstring& scheme, const mstring& out_folder)
{
    mvector<unique_ptr<source> > sources;
    {
        auto sc = read_file(scheme.c_str());
        auto it = sc.begin(), ie = sc.end();
        while(it != ie)
        {
            while(it != ie && *it == '\n')
                ++it;
            if(it == ie)
                break;
            auto ii = find(it, ie, '\n');
            if(*it != ';') {
                mvector<str_holder> vs = split(str_holder(it, ii), ';');
                if(vs.size() != 4)
                    throw mexception(es() % "bad schema source: " % mstring(it, ii));
                mvector<str_holder> tables = split(vs[3], ',');

                unique_ptr<source> s(new source(out_folder + "/" + vs[0]));
                s->ns = namespace_type(vs[1], vs[2]);
                std::copy(tables.begin(), tables.end(), std::inserter(s->tables, s->tables.begin()));
                s->write_head(vs[0]);
                sources.push_back(std::move(s));
            }
            it = ii;
        }
    }
    
    mvector<mstring> files = list_folder(ini_folder);
    for(const mstring& fname: files) {
        auto data = read_file((ini_folder + "/" + fname).c_str());
        for(unique_ptr<source>& s: sources) {
            s->cur_namespace = namespace_type();
            parse_file(*s, data, fname);
        }
    }

    b_stream str;
    for(uint32_t i = 0; i != sources.size(); ++i) {
        source& s = *sources[i];
        if(i)
            str << ",";
        str << s.name;
        s.check_fill();
        s.write_cpp_tail();
    }
    cout() << "sources " << str.str() << " successfully saved" << endl;
}

int main(int argc, char** arg)
{
    try {
        auto argv = init_params(argc, arg, false);
        if(argc != 2 && argc != 4) {
            cout() << "Usage:" << endl
                << "    ./plaza_templater scheme_folder" << endl
                << "    ./plaza_templater scheme_folder part_scheme_file output_folder" << endl;
            return 1;
        }
        if(argc == 2)
            parse_all(argv[1], mstring("plaza_template.inl"));
        else
            proceed_selected(argv[1], argv[2], argv[3]);

        return 0;
    }
    catch(exception& e) {
        std::cerr << "exception: " << e.what() << endl;
    	return 2;
    }
}

