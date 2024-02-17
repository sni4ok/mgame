/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "array.hpp"

uint32_t get_thread_id();

template<typename node, bool use_tss>
struct data_tss;

template<typename node>
struct data_tss<node, true>
{
    static const uint32_t tss_max_threads = 100;

    mutable carray<node, tss_max_threads> nodes;

    node& get_data() const {
        return nodes[get_thread_id()];
    }
};

template<typename node>
struct data_tss<node, false> 
{
    mutable node ptr;

    node& get_data() const {
        return ptr;
    }
};

template<typename node>
struct list_iterator
{
    node* n;

    list_iterator() : n() {
    }
    list_iterator(node* n) : n(n) {
    }
    list_iterator& operator++() {
        n = n->next;
        return *this;
    }
    list_iterator& operator--() {
        n = n->prev;
        return *this;
    }
    auto& operator*() {
        return n->value;
    }
    auto* operator->() {
        return &n->value;
    }
    bool operator!=(const list_iterator& r) const {
        return n != r.n;
    }
    bool operator==(const list_iterator& r) const {
        return n == r.n;
    }
};

template<typename type, bool use_mt, bool use_tss, bool delete_on_exit, typename node_type>
struct list_base : data_tss<node_type*, use_tss>
{
    typedef node_type node;

    critical_section cs;

    list_base() {
        static_assert(int(use_mt) + int(use_tss) <= 1);
        auto m = []() {
            node* n = (node*)::malloc(sizeof(node));
            memset((void*)n, 0, sizeof(node));
            return n;
        };
        if constexpr(use_tss)
            for(node*& n: this->nodes)
                n = m();
        else
            this->ptr = m();
    }
    static type* to_type(node* n) {
        return &n->value;
    }
    static node* to_node(type* p) {
        return (node*)p;
    }
    template<typename ... params>
    static type* alloc_node(params&& ... args) {
        return to_type(new node({args...}, {nullptr}));
    }

    typedef list_iterator<const node> const_iterator;
    typedef list_iterator<node> iterator;

    iterator begin() {
        return iterator(this->get_data()->next);
    }
    const_iterator begin() const {
        return const_iterator(this->get_data()->next);
    }
    iterator end() {
        return iterator();
    }
    const_iterator end() const {
        return const_iterator();
    }
    ~list_base() {
        if constexpr(delete_on_exit) {
            auto del_func = [&](node* ptr) {
                iterator i(ptr->next), t = i, e = iterator();
                while(i != e) {
                    ++t;
                    delete to_node(&(*i));
                    i = t;
                }
                ::free(ptr);
            };
            if constexpr(use_tss)
                for(auto& v : this->nodes)
                    del_func(v);
            else
                del_func(this->ptr);
        }
    }
};

template<typename type>
struct forward_list_node
{
    type value;
    forward_list_node* next;
};

template<typename type, bool use_mt = true, bool use_tss = false, bool delete_on_exit = true, typename node_type = forward_list_node<type> >
struct forward_list : list_base<type, use_mt, use_tss, delete_on_exit, node_type> 
{
    typedef list_base<type, use_mt, use_tss, delete_on_exit, node_type> base;
    typedef node_type node;

    void push_front(type* p) {
        node* n = base::to_node(p);
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        n->next = ptr->next;
        ptr->next = n;

        if constexpr(use_mt)
            this->cs.unlock();
    }
    type* pop_front() {
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        node* p = ptr->next;
        if(p)
            ptr->next = p->next;

        if constexpr(use_mt)
            this->cs.unlock();

        return base::to_type(p);
    }
    void push(type* p) {
        push_front(p);
    }
    type* pop() {
        return this->pop_front();
    }
};

template<typename type>
struct blist_node
{
    type value;
    blist_node *prev, *next;
};

template<typename type, bool use_mt, bool use_tss, bool delete_on_exit, typename node_type = blist_node<type> >
struct blist : list_base<type, use_mt, use_tss, delete_on_exit, node_type> 
{
    typedef list_base<type, use_mt, use_tss, delete_on_exit, node_type> base;
    typedef node_type node;

    void push_front(type* p) {
        node* n = base::to_node(p);
        n->prev = nullptr;
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        n->next = ptr->next;
        if(n->next)
            n->next->prev = n;
        else if(!ptr->prev)
            ptr->prev = n;
        ptr->next = n;

        if constexpr(use_mt)
            this->cs.unlock();
    }
    void push_back(type* p) {
        node* n = base::to_node(p);
        n->next = nullptr;
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        n->prev = ptr->prev;
        if(n->prev)
            n->prev->next = n;
        else if(!ptr->next)
            ptr->next = n;
        ptr->prev = n;

        if constexpr(use_mt)
            this->cs.unlock();
    }
    type* pop_front() {
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        node* p = ptr->next;
        if(p) {
            ptr->next = p->next;
            if(ptr->next)
                ptr->next->prev = nullptr;
            else
                ptr->prev = nullptr;
        }

        if constexpr(use_mt)
            this->cs.unlock();

        return base::to_type(p);
    }
    type* pop_back() {
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        node* p = ptr->prev;
        if(p) {
            ptr->prev = p->prev;
            if(ptr->prev)
                ptr->prev->next = nullptr;
            else
                ptr->next = nullptr;
        }

        if constexpr(use_mt)
            this->cs.unlock();

        return base::to_type(p);
    }
    void push(type* p) {
        this->push_back(p);
        //this->push_front(p);
    }
    type* pop() {
        return this->pop_front();
        //return this->pop_back();
    }
};

template<typename type, bool use_mt, bool use_tss, bool delete_on_exit, typename node_type = type>
struct tss_pvector : data_tss<mvector<type*>, use_tss>
{
    typedef type node;

    tss_pvector() {
        static_assert(!use_mt && "todo");
    }
    static type* to_type(type* p) {
        return p;
    }
    static type* to_node(type* p) {
        return p;
    }
    void push(type* p) {
        this->get_data().push_back(p);
    }
    type* pop() {
        auto& v = this->get_data();
        if(v.empty())
            return nullptr;
        type* p = v.back();
        v.pop_back();
        return p;
    }
    ~tss_pvector() {
        if constexpr(delete_on_exit) {
            auto del_func = [&](mvector<type*>& data) {
                for(type* p: data)
                    delete p;
            };
            if constexpr(use_tss)
                for(auto& v : this->nodes)
                    del_func(v);
            else
                del_func(this->get_data());
        }
    }
};

