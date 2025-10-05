/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "profiler.hpp"
#include "string.hpp"
#include "atomic.hpp"
#include "list.hpp"

//with follower can work one writer and many readers without synchronization
template<typename type, u32 block_size = 65536, u32 num_blocks = 16384,
    bool enable_run_once = true>
class follower
{
    typedef type* bulks_type[num_blocks];

    bulks_type bulks;
    mutable u32 num_elems;
    type* ptr;

public:
    typedef type value_type;

    follower() : num_elems(), ptr()
    {
        if constexpr(enable_run_once)
            run_once();
    }
    follower(const follower&) = delete;
    ~follower()
    {
        u32 elems = atomic_load(num_elems);
        u32 end_bulk = elems / block_size;
        u32 end_element = elems % block_size;
        if(end_element)
            ++end_bulk;
        for(u32 i = 0; i != end_bulk; ++i)
            delete[] bulks[i];
        if(ptr)
            delete[] ptr;
    }
    bool run_once() requires(enable_run_once)
    {
        if(!ptr)
        {
            type* p = new type[block_size];
            p = atomic_exchange(&ptr, p);
            if(p)
            {
                MPROFILE("follower::run_once() race")
                delete[] p;
            }
            return true;
        }
        return false;
    }
    void push_back(type&& value)
    {
        u32 elems = atomic_load(num_elems);
        u32 end_bulk = elems / block_size;
        u32 end_element = elems % block_size;

        if(!end_element)
        {
            type* p = nullptr;
            if constexpr(enable_run_once)
                p = atomic_exchange(&ptr, (type*)nullptr);

            if(p)
            {
                MPROFILE("follower::new_atomic")
                bulks[end_bulk] = p;
            }
            else
            {
                MPROFILE("follower::new")
                bulks[end_bulk] = new type[block_size];
            }
        }

        (bulks[end_bulk])[end_element] = move(value);
        atomic_add(num_elems, 1u);
    }
    void push_back(const type& value)
    {
        push_back(type(value));
    }
    class const_iterator
    {
        const bulks_type* bulks;
        u32 element;

        friend class follower;

        const type& dereference() const
        {
            u32 end_bulk = element / block_size;
            u32 end_element = element % block_size;
            return ((*bulks)[end_bulk])[end_element];
        }
        const_iterator(const bulks_type& bulks, u32 element)
            : bulks(&bulks), element(element)
        {
        }

    public:
        const_iterator()
        {
        }
        bool operator==(const const_iterator& r) const
        {
            return element == r.element;
        }
        const_iterator& operator++()
        {
            ++element;
            return *this;
        }
        const_iterator& operator--()
        {
            --element;
            return *this;
        }
        const_iterator operator-(i32 diff) const
        {
            const_iterator ret(*bulks, element);
            ret -= diff;
            return ret;
        }
        const_iterator operator+(i32 diff) const
        {
            const_iterator ret(*bulks, element);
            ret += diff;
            return ret;
        }
        const_iterator& operator-=(i32 diff)
        {
            element -= diff;
            return *this;
        }
        const_iterator& operator+=(i32 diff)
        {
            element += diff;
            return *this;
        }
        i32 operator-(const const_iterator& r) const
        {
            return element - r.element;
        }
        const type& operator*() const
        {
            return dereference();
        }
        const type* operator->() const
        {
            const type& t = dereference();
            return &t;
        }
    };
    const_iterator begin() const
    {
        return const_iterator(bulks, 0);
    }
    const_iterator end() const
    {
        return const_iterator(bulks, atomic_load(num_elems));
    }
    const type& operator[](u32 idx) const
    {
        return *(begin() + idx);
    }
    u32 size() const
    {
        return atomic_load(num_elems);
    }
};

template<typename type, typename base>
struct emplace_decl
{
    template<typename ... par>
    void emplace(par&& ... args)
    {
        type* p = ((base*)this)->alloc();
        new(p)type(args...);
        ((base*)this)->push(p);
    }
    template<typename ... par>
    void emplace_front(par&& ... args)
    {
        type* p = ((base*)this)->alloc();
        new(p)type(args...);
        ((base*)this)->push_front(p);
    }
    template<typename ... par>
    void emplace_back(par&& ... args)
    {
        type* p = ((base*)this)->alloc();
        new(p)type(args...);
        ((base*)this)->push_back(p);
    }
};

template<typename type, u32 max_size>
struct cas_forward : emplace_decl<type, cas_forward<type, max_size> >
{
    static const bool use_blist = false;

    struct node_type
    {
        char value_buf[sizeof(type)];
    };

    struct id_type
    {
        union
        {
            u64 id;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
            struct
            {
                u16 next, a_next, salt;
            };
#pragma GCC diagnostic pop
        };
    };

    struct node : node_type, id_type
    {
    };

    node nodes[max_size + 1];

    cas_forward() : nodes()
    {
        static_assert(max_size && max_size < limits<u16>::max);
        static_assert(sizeof(node) == 8 + sizeof(type) +
            ((sizeof(type) % 8) ? (8 - sizeof(type) % 8) : 0));
        for(u32 i = 1; i != max_size + 1; ++i)
            free_impl(to_type(nodes + i));
    }
    cas_forward(str_holder) : cas_forward()
    {
    }
    ~cas_forward()
    {
        for(const type& t: (*this))
            t.~type();
    }
    type* to_type(node* n)
    {
        assert(n > nodes && n < nodes + max_size + 1);
        return (type*)n->value_buf;
    }
    node* to_node(type* p)
    {
        assert(p);
        node* n = (node*)(p);
        assert(n > nodes && n < nodes + max_size + 1);
        return n;
    }
    void free_impl(type* p)
    {
        node* n = to_node(p);
        for(;;)
        {
            u64 from = atomic_load(nodes->id);
            id_type to = {from};
            to.a_next = n - nodes;
            n->id = from; //
            ++to.salt;
            if(atomic_compare_exchange(nodes->id, from, to.id))
                break;
        }
    }
    void free(type* p)
    {
        p->~type();
        free_impl(p);
    }
    void push_front(type* p)
    {
        node* n = to_node(p);
        for(;;)
        {
            u64 from = atomic_load(nodes->id, __ATOMIC_ACQUIRE);
            id_type to = {from};
            to.next = n - nodes;

            n->id = from;
            ++to.salt;
            if(atomic_compare_exchange(nodes->id, from, to.id, __ATOMIC_RELEASE))
                break;
        }
    }
    type* alloc()
    {
        for(;;)
        {
            u64 from = atomic_load(nodes->id, __ATOMIC_ACQUIRE);
            id_type to = {from};
            if(!to.a_next)
                throw mexception(es() % "cas_array<type," % max_size % "> bad_alloc");
            u64 a_next = atomic_load((nodes + to.a_next)->id);
            id_type n = {a_next};
            u16 nn = to.a_next;
            to.a_next = n.a_next;
            ++to.salt;
            if(atomic_compare_exchange(nodes->id, from, to.id, __ATOMIC_RELEASE))
                return to_type(nodes + nn);
        }
    }
    type* alloc_new()
    {
        type* p = alloc();
        new (p)type();
        return p;
    }
    type* pop_front()
    {
        for(;;)
        {
            u64 from = atomic_load(nodes->id, __ATOMIC_ACQUIRE);
            id_type to = {from};
            if(!to.next)
                return nullptr;

            u16 nn = to.next;
            u64 next = atomic_load((nodes + nn)->id);

            id_type n = {next};

            to.next = n.next;
            ++to.salt;
            bool succ = atomic_compare_exchange(nodes->id, from, to.id, __ATOMIC_RELEASE);
            if(succ)
                return to_type(nodes + nn);
        }
    }
    void push(type* p)
    {
        push_front(p);
    }
    type* pop()
    {
        return pop_front();
    }
    static constexpr u32 run_once()
    {
        return 0;
    }

    struct const_iterator : list_iterator<const node, false>
    {
        const node* nodes;

        const_iterator(const node* nodes) : list_iterator<const node, false>(nodes, true),
            nodes(nodes)
        {
        }
        const_iterator(const node* nodes, node* n) : list_iterator<const node, false>(n),
            nodes(nodes)
        {
        }
        const_iterator& operator++()
        {
            if(!this->n->next)
                this->n = nullptr;
            else
                this->n = nodes + this->n->next;
            return *this;
        }
        const type& operator*() const
        {
            return *((type*)this->n->value_buf);
        }
        const type* operator->() const
        {
            return (type*)this->n->value_buf;
        }
    };

    const_iterator begin() const
    {
        return const_iterator(nodes, (node*)nodes + nodes->next);
    }
    const_iterator end() const
    {
        return const_iterator(nodes);
    }
};

template<typename type, u16 capacity_v, type empty_v = type(), bool use_mt = true>
struct rbuffer
{
    mutable critical_section cs;

    u16 f, t;
    type values[capacity_v];

    rbuffer() : f(), t()
    {
        fill(values, values + capacity_v, empty_v);
    }

    void push_back(type v)
    {
        critical_section::mt_lock<use_mt> lock(cs);

        u16 tf;
        if(t == capacity_v)
        {
            tf = 0;
            t = 0;
        }
        else
        {
            if(t >= f)
                tf = t;
            else
                tf = t + 1;
            t = t + 1;
        }

        assert(t != f && "rbuffer::push_back overloaded");
        values[tf] = v;
    }
    type pop_front()
    {
        critical_section::mt_lock<use_mt> lock(cs);

        if(f == t)
            return empty_v;

        type v = values[f];

        values[f] = empty_v;
        if(f == capacity_v - 1)
        {
            f = 0;
            if(t == capacity_v)
                t = 0;
            else
                ++t;
        }
        else
            f = f + 1;

        return v;
    }
    void push(type v)
    {
        push_back(v);
    }
    type pop()
    {
        return pop_front();
    }
    type front() const
    {
        critical_section::mt_lock<use_mt> lock(cs);

        if(f == t)
            return empty_v;

        return values[f];
    }
    u16 __size() const
    {
        if(t != capacity_v)
        {
            if(t > f)
                return t - f;
            if(t < f)
                return capacity_v - f + t + 1;
        }
        else
            return t - f;

        return 0;
    }
    u16 size() const
    {
        critical_section::mt_lock<use_mt> lock(cs);
        return __size();
    }
    bool empty() const
    {
        return !size();
    }
    static constexpr u16 capacity()
    {
        return capacity_v;
    }
};

template<typename type, u32 capacity_v, bool use_mt = true>
struct rbuffer_list : rbuffer<type*, capacity_v, nullptr, use_mt>
{
    static const bool use_blist = false;
    typedef rbuffer<type*, capacity_v, nullptr, use_mt> base;

    type* alloc()
    {
        return new type();
    }
    void free(type* v)
    {
        delete v;
    }
    ~rbuffer_list()
    {
        type* p;
        while((p = this->pop()))
            free(p);
    }
    static constexpr u32 run_once()
    {
        return 0;
    }
};

namespace alloc_params
{
    struct container_tag
    {
        int value;
    };

    static constexpr container_tag vector_t({0});
    static constexpr container_tag forward_list_t({1});
    static constexpr container_tag blist_t({2});

    struct fast_alloc_params
    {
        bool use_mt;
        bool use_tss;

        u32 pre_alloc = 0;
        u32 max_size = 0;

        bool use_malloc = false;

        int cont_type = 2; //tss_pvector 0, forward_list 1, blist 2
        int allocator_cont = 1;

        const constexpr fast_alloc_params operator()(u32 pre_alloc,
            u32 max_size = 0) const
        {
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size,
                use_malloc, cont_type, allocator_cont);
        }
        const constexpr fast_alloc_params malloc(bool use_malloc = true) const
        {
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size,
                use_malloc, cont_type, allocator_cont);
        }
        const constexpr fast_alloc_params operator()(container_tag cont,
            container_tag ac = forward_list_t) const
        {
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size,
                use_malloc, cont.value, ac.value);
        }
        constexpr int node_type() const
        {
            return max(cont_type, allocator_cont);
        }
    };

    static constexpr fast_alloc_params st({false, false});
    static constexpr fast_alloc_params mt({true, false});
    static constexpr fast_alloc_params mt_tss({true, true});
}

using namespace alloc_params;

static inline void count_slow()
{
    MPROFILE("fast_alloc::alloc() slow")
}

template<typename type, fast_alloc_params params = mt_tss,
    template<typename, bool, bool, bool, typename> typename nodes_type_ = forward_list,
    typename node = forward_list_node<type, params.use_tss> >
struct fast_alloc
{
    static const bool use_mt = params.use_mt;
    static const bool use_tss = params.use_tss;
    static const u32 pre_alloc = use_tss ? 0 : params.pre_alloc;
    static const u32 max_size = use_tss ? 0 : params.max_size;

    typedef nodes_type_<type, use_mt, use_tss, true, node> nodes_type;
    nodes_type nodes;
    u32 size;

    type* alloc_impl()
    {
        node* n = fast_alloc_alloc<node>();
        if constexpr(use_tss)
            nodes.set_tss_ptr(n);
        return nodes_type::to_type(n);
    }
    fast_alloc() : size()
    {
        if constexpr(pre_alloc)
        {
            for(u32 i = 0; i != pre_alloc; ++i)
            {
                type* p = alloc_impl();
                new(p)type();
                nodes.push(p);
            }
        }
        if constexpr(max_size)
            size += pre_alloc;
    }
    type* alloc_new()
    {
        type* p = nodes.pop();
        if(p)
        {
            if constexpr(max_size)
                atomic_sub(size, 1u);
        }
        else
        {
            count_slow();
            p = alloc_impl();
            new (p)type;
        }
        return p;
    }
    type* alloc()
    {
        type* p = nodes.pop();
        if(p)
        {
            p->~type();
            if constexpr(max_size)
                atomic_sub(size, 1u);
        }
        else
        {
            count_slow();
            p = alloc_impl();
        }
        return p;
    }
    void free(type* p)
    {
        if constexpr(max_size)
        {
            u32 size_ = atomic_load(size);
            if(size_ < max_size)
            {
                nodes.push(p);
                atomic_add(size, 1u);
            }
            else
            {
                MPROFILE("fast_alloc::free() slow")
                fast_alloc_delete(nodes_type::to_node(p));
                return;
            }
        }
        else
            nodes.push(p);
    }
    u32 run_once()
    {
        if constexpr(max_size == 0)
            return 0;

        MPROFILE("fast_alloc::run_once()")
        u32 ret = 0;

        while(atomic_load(size) > max_size)
        {
            MPROFILE("fast_alloc::run_once() pop")
            type* p = nodes.pop();
            if(p)
            {
                atomic_sub(size, 1u);
                fast_alloc_delete(nodes_type::to_node(p));
                ++ret;
            }
            else
            {
                MPROFILE("fast_alloc::run_once() pop race")
                return ret;
            }
        }
        while(atomic_load(size) < pre_alloc)
        {
            MPROFILE("fast_alloc::run_once() push")
            nodes.push(alloc_impl());
            atomic_add(size, 1u);
            ++ret;
        }
        return ret;
    }
};

template<typename type, typename node_type>
struct malloc_alloc
{
    type* alloc_new()
    {
        type* p = (type*)fast_alloc_alloc<node_type>();
        new (p)type;
        return p;
    }
    type* alloc()
    {
        return (type*)fast_alloc_alloc<node_type>();
    }
    void free(type* n)
    {
        fast_alloc_delete((node_type*)n);
    }
    static constexpr u32 run_once()
    {
        return 0;
    }
};

template<int index, int index_from, typename ... params>
struct conditional_multi_f;

template<int index, int index_from, typename param>
struct conditional_multi_f<index, index_from, param>
{
    typedef conditional_t<index == index_from, param, void> type;
};

template<int index, int index_from, typename param, typename ... params>
struct conditional_multi_f<index, index_from, param, params...>
{
    typedef conditional_t<index == index_from, param,
        typename conditional_multi_f<index, index_from + 1, params...>::type> type;
};

template<int index, typename param, typename ... params>
struct conditional_multi : conditional_multi_f<index, 0, param, params...>
{
};

template<int index, typename param, typename ... params>
using conditional_multi_t = typename conditional_multi<index, param, params...>::type;

template<typename type, bool use_tss, int t>
struct node_type
{
    typedef conditional_multi_t<t, type, forward_list_node<type, use_tss>,
        blist_node<type, use_tss> > node;
};

template<typename type_, typename node, fast_alloc_params params>
struct container_type
{
    typedef conditional_multi_t<params.cont_type,
        tss_pvector<type_, params.use_mt, false, true, node>,
        forward_list<type_, params.use_mt, false, true, node>,
        blist<type_, params.use_mt, false, true, node> > type;
};

template<typename type_, typename node, fast_alloc_params params>
struct allocator_type
{
    typedef
    conditional_t<params.use_malloc, malloc_alloc<type_, node>,
        conditional_multi_t<params.allocator_cont,
            fast_alloc<type_, params, tss_pvector, node>,
            fast_alloc<type_, params, forward_list, node>,
            fast_alloc<type_, params, blist, node>
    > > type;
};

template<typename type, fast_alloc_params params = mt_tss,
    typename node = node_type<type, params.use_tss, params.node_type()>::node>
struct fast_alloc_list :
    allocator_type<type, node, params>::type,
    container_type<type, node, params>::type,
    emplace_decl<type, fast_alloc_list<type, params, node> >
{
    fast_alloc_list()
    {
    }
    fast_alloc_list(str_holder)
    {
    }
};

