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

template<typename node, bool blist>
struct list_iterator
{
    node* n;
    bool e;

    list_iterator() : n() {
    }
    list_iterator(node* n, bool e = false) : n(n), e(e) {
    }
    list_iterator& operator++() {
        n = n->next;
        return *this;
    }
    list_iterator& operator--() requires(blist) {
        n = n->prev;
        return *this;
    }
    auto& operator*() {
        return n->value;
    }
    auto* operator->() {
        return &n->value;
    }
    bool operator==(const list_iterator& r) const {
        if(e != r.e && (!n || !r.n))
            return true;
        return n->next == r.n->next;
    }
    bool operator!=(const list_iterator& r) const {
        return !(*this == r);
    }
};

#include "mlog.hpp"

template<typename node>
node* fast_alloc_alloc()
{
    node* n = (node*)malloc(sizeof(node));
    if(!n)
        throw std::bad_alloc();
    return n;
}

template<typename node>
void fast_alloc_free(node* n)
{
    ::free(n);
}

template<typename node>
void fast_alloc_delete(node* n)
{
    n->~node();
    ::free(n);
}

template<typename type, bool use_mt, bool use_tss, bool delete_on_exit, typename node_type, bool blist = false>
struct list_base : data_tss<node_type*, use_tss>
{
    typedef node_type node;

    static const bool use_blist = blist;

    critical_section cs;

    list_base() {
        auto m = []() {
            node* n = fast_alloc_alloc<node>();
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
    node* get_data_impl(node* n) {
        if constexpr(use_tss)
            return n->ptr;

        return this->get_data();
    }
    void set_tss_ptr(node *n) requires(use_tss){
        n->ptr = this->get_data();
    }

    template<typename ... params>
    static type* alloc_node(params&& ... args) {
        node* n = fast_alloc_alloc<node>();
        type* p = to_type(n);
        new(p)type(args...);
        return p;
    }
    bool erase(type* p) { //liniar complexity
        node* n = to_node(p);

        if constexpr(use_mt)
            this->cs.lock();

        node* prev = this->get_data();

        if constexpr(blist)
            if(prev->prev == n)
                prev->prev = prev->prev->prev;

        node* ne = prev->next;

        while(ne) {
            if(ne == n) {
                if(prev)
                    prev->next = n->next;

                if constexpr(blist)
                    if(n->next)
                        n->next->prev = prev;

                if constexpr(use_mt)
                    this->cs.unlock();

                if constexpr(delete_on_exit)
                    fast_alloc_delete(n);

                return true;
            }

            prev = ne;
            ne = ne->next;
        }

        if constexpr(use_mt)
            this->cs.unlock();

        return false;
    }

    typedef list_iterator<const node, blist> const_iterator;
    typedef list_iterator<node, blist> iterator;

    iterator begin() {
        return iterator(this->get_data()->next);
    }
    const_iterator begin() const {
        return const_iterator(this->get_data()->next);
    }
    iterator end() {
        return iterator(this->get_data(), true);
    }
    const_iterator end() const {
        return const_iterator(this->get_data(), true);
    }
    ~list_base() {
        if constexpr(delete_on_exit) {
            auto del_func = [&](node* ptr) {
                iterator i(ptr->next), t = i, e = iterator(ptr, true);
                while(i != e) {
                    ++t;
                    fast_alloc_delete(to_node(&(*i)));
                    i = t;
                }
                fast_alloc_free(ptr);
            };
            if constexpr(use_tss)
                for(auto& v : this->nodes)
                    del_func(v);
            else
                del_func(this->ptr);
        }
    }
};

template<typename type, bool use_tss>
struct forward_list_node;

template<typename type>
struct forward_list_node<type, false>
{
    type value;
    forward_list_node* next;
};

template<typename type>
struct forward_list_node<type, true>
{
    type value;
    forward_list_node* next;
    forward_list_node* ptr;
};

template<typename type, bool use_mt = true, bool use_tss = false, bool delete_on_exit = true, typename node_type = forward_list_node<type, use_tss> >
struct forward_list : list_base<type, use_mt, use_tss, delete_on_exit, node_type> 
{
    typedef list_base<type, use_mt, use_tss, delete_on_exit, node_type> base;
    typedef node_type node;

    void push_front(type* p) {
        node* n = base::to_node(p);
        node* ptr = this->get_data_impl(n);

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

        node* n = ptr->next;
        if(n)
            ptr->next = n->next;

        if constexpr(use_mt)
            this->cs.unlock();

        if constexpr(use_tss)
            if(n)
                n->ptr = ptr;

        return base::to_type(n);
    }
    void push(type* p) {
        push_front(p);
    }
    type* pop() {
        return this->pop_front();
    }
};

template<typename type, bool use_tss>
struct blist_node;

template<typename type>
struct blist_node<type, false>
{
    type value;
    blist_node *prev, *next;
};

template<typename type>
struct blist_node<type, true>
{
    type value;
    blist_node *prev, *next;
    blist_node *ptr;
};

template<typename type, bool use_mt, bool use_tss, bool delete_on_exit, typename node_type = blist_node<type, use_tss> >
struct blist : list_base<type, use_mt, use_tss, delete_on_exit, node_type, true> 
{
    typedef list_base<type, use_mt, use_tss, delete_on_exit, node_type, true> base;
    typedef node_type node;

    void push_front(type* p) {
        node* n = base::to_node(p);
        node* ptr = this->get_data_impl(n);

        n->prev = nullptr;

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
        node* ptr = this->get_data_impl(n);

        n->next = nullptr;

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

        node* n = ptr->next;
        if(n) {
            ptr->next = n->next;
            if(ptr->next)
                ptr->next->prev = nullptr;
            else
                ptr->prev = nullptr;
        }

        if constexpr(use_mt)
            this->cs.unlock();

        if constexpr(use_tss)
            if(n)
                n->ptr = ptr;

        return base::to_type(n);
    }
    type* pop_back() {
        node* ptr = this->get_data();

        if constexpr(use_mt)
            this->cs.lock();

        node* n = ptr->prev;
        if(n) {
            ptr->prev = n->prev;
            if(ptr->prev)
                ptr->prev->next = nullptr;
            else
                ptr->next = nullptr;
        }

        if constexpr(use_mt)
            this->cs.unlock();

        if constexpr(use_tss)
            if(n)
                n->ptr = ptr;

        return base::to_type(n);
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
        //static_assert(!use_mt && "todo");
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
    void set_tss_ptr(node*) requires(use_tss){
    }

    typedef pvector<type>::iterator iterator;
    typedef pvector<type>::const_iterator const_iterator;

    iterator begin() {
        return iterator(this->get_data().begin());
    }
    const_iterator begin() const {
        return const_iterator(this->get_data().begin());
    }
    iterator end() {
        return iterator(this->get_data().end());
    }
    const_iterator end() const {
        return const_iterator(this->get_data().end());
    }
    ~tss_pvector() {
        if constexpr(delete_on_exit) {
            auto del_func = [&](mvector<type*>& data) {
                for(type* p: data)
                    fast_alloc_delete(to_node(p));
            };
            if constexpr(use_tss)
                for(auto& v : this->nodes)
                    del_func(v);
            else
                del_func(this->get_data());
        }
    }
};

