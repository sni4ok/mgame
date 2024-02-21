/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "profiler.hpp"
#include "string.hpp"
#include "atomic.hpp"
#include "list.hpp"

#include <unistd.h>

//with follower can work one writer and many readers without synchronization
template<typename type, uint32_t block_size = 65536, uint32_t num_blocks = 16384, bool enable_run_once = true>
class follower
{
    typedef type* bulks_type[num_blocks];

    bulks_type bulks;
    mutable uint32_t num_elems;
    type* ptr;

public:
    typedef type value_type;

    follower() : num_elems(), ptr() {
        if constexpr(enable_run_once)
            run_once();
    }
    follower(const follower&) = delete;
    ~follower() {
        uint32_t elems = atomic_load(num_elems);
        uint32_t end_bulk = elems / block_size;
        uint32_t end_element = elems % block_size;
        if(end_element)
            ++end_bulk;
        for(uint32_t i = 0; i != end_bulk; ++i)
            delete[] bulks[i];
        if(ptr)
            delete[] ptr;
    }
    bool run_once() requires(enable_run_once) {
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
    void push_back(type&& value) {
        uint32_t elems = atomic_load(num_elems);
        uint32_t end_bulk = elems / block_size;
        uint32_t end_element = elems % block_size;

        if(!end_element) {
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

        (bulks[end_bulk])[end_element] = std::move(value);
        atomic_add(num_elems, 1);
    }
    void push_back(const type& value) {
        push_back(type(value));
    }
    class const_iterator
    {
        const bulks_type* bulks;
        uint32_t element;

        friend class follower;

        const type& dereference() const { 
            uint32_t end_bulk = element / block_size;
            uint32_t end_element = element % block_size;
            return ((*bulks)[end_bulk])[end_element];
        }
        const_iterator(const bulks_type& bulks, uint32_t element) : bulks(&bulks), element(element){
        }
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = type;
        using difference_type = int32_t;
        using pointer = type*;
        using reference = type&;

        const_iterator(){}
        bool operator==(const const_iterator& r) const {
            return element == r.element;
        }
        bool operator!=(const const_iterator& r) const {
            return element != r.element;
        }
        const_iterator& operator++() {
            ++element;
            return *this;
        }
        const_iterator& operator--() {
            --element;
            return *this;
        }
        const_iterator operator-(int32_t diff) const {
            const_iterator ret(*bulks, element);
            ret -= diff;
            return ret;
        }
        const_iterator operator+(int32_t diff) const {
            const_iterator ret(*bulks, element);
            ret += diff;
            return ret;
        }
        const_iterator& operator-=(int32_t diff) {
            element -= diff;
            return *this;
        }
        const_iterator& operator+=(int32_t diff) {
            element += diff;
            return *this;
        }
        int32_t operator-(const const_iterator& r) const {
            return element - r.element;
        }
        const type& operator*() const {
            return dereference();
        }
        const type* operator->() const {
            const type& t = dereference();
            return &t;
        }
    };
    const_iterator begin() const {
        return const_iterator(bulks, 0);
    }
    const_iterator end() const {
        return const_iterator(bulks, atomic_load(num_elems));
    }
    const type& operator[](uint32_t idx) const {
        return *(begin() + idx);
    }
    uint32_t size() const {
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

template<typename type, uint32_t max_size, bool blist = true>
struct cas_array : emplace_decl<type, cas_array<type, max_size, blist> >
{
    struct node_type
    {
        char value_buf[sizeof(type)];
    };

    struct id_type
    {
        union {
            uint64_t id;

            struct {
                uint16_t prev, next;
                uint16_t a_next;
                uint16_t salt;
            };
        };
    };

    struct node : node_type, id_type
    {
    };

    node nodes[max_size + 1];

    static const uint64_t push_lock = -1, pop_lock = -2, locks_from = -2;

    cas_array() : nodes() {
        static_assert(max_size && max_size < std::numeric_limits<uint16_t>::max());
        static_assert(sizeof(node) == 8 + sizeof(type) + ((sizeof(type) % 8) ? (8 - sizeof(type) % 8) : 0));
        for(uint32_t i = 1; i != max_size + 1; ++i)
            free(to_type(nodes + i));
    }
    cas_array(str_holder) : cas_array() {
    }
    ~cas_array() {
        for(type& t: (*this))
            t.~type();
    }
    type* to_type(node* n) {
        assert(n > nodes && n < nodes + max_size + 1);
        return (type*)n->value_buf;
    }
    node* to_node(type* p) {
        assert(p);
        node* n = (node*)(p);
        assert(n > nodes && n < nodes + max_size + 1);
        return n;
    }
    void free(type* p) {
        node* n = to_node(p);
        for(;;) {
            uint64_t from = atomic_load(nodes->id);
            id_type to = {from};
            to.a_next = n - nodes;
            n->id = from; //
            ++to.salt;
            if(atomic_compare_exchange(nodes->id, from, to.id))
                break;
        }
    }
    void push_front(type* p) requires(!blist){
        node* n = to_node(p);
        for(;;) {
            uint64_t from = atomic_load(nodes->id);
            id_type to = {from};
            //uint16_t next = to.next;
            to.next = n - nodes;

            //if constexpr(blist) {
            //    if(!next)
            //        to.prev = to.next;
            //}
            n->id = from;
            //if constexpr(blist)
            //    n->prev = 0;
            ++to.salt;
            if(atomic_compare_exchange(nodes->id, from, to.id))
            {
                /*if constexpr(blist) {
                    if(next) {
                        if(!atomic_compare_exchange((nodes + next)->prev, uint16_t(), to.next)) {
                            //assert(false);
                            MPROFILE("cas_array::push_front() race");
                        }
                    }
                }*/
                break;
            }
        }
    }

    void push_back(type* p) requires(blist) {
        node* n = to_node(p);
        for(;;) {
            uint64_t from = atomic_load(nodes->id);
            id_type to = {from};
            uint16_t prev = to.prev;

            id_type prev_to;
            if(prev) {
                prev_to.id = atomic_load((nodes + prev)->id);
                if(prev_to.next || prev_to.id >= locks_from)
                    continue;
                if(!atomic_compare_exchange((nodes + prev)->id, prev_to.id, push_lock))
                    continue;
            }
            {
                id_type f = {from};
                f.next = 0;
                atomic_store(n->id, f.id);
            }
            to.prev = n - nodes;
            if(to.next == 0)
                to.next = to.prev;
            ++to.salt;
            bool succ = (atomic_compare_exchange(nodes->id, from, to.id));
            if(prev) {
                if(succ)
                    prev_to.next = to.prev;
                atomic_store((nodes + prev)->id, prev_to.id);
            }
            if(succ)
                break;
        }
    }
    type* alloc() {
        for(;;) {
            uint64_t from = atomic_load(nodes->id);
            id_type to = {from};
            if(!to.a_next)
                throw mexception(es() % "cas_array<type," % max_size % "> bad_alloc");
            uint64_t a_next = atomic_load((nodes + to.a_next)->id);
            id_type n = {a_next};
            uint16_t nn = to.a_next;
            to.a_next = n.a_next;
            ++to.salt;
            if(atomic_compare_exchange(nodes->id, from, to.id))
                return to_type(nodes + nn);
        }
    }
    type* pop_front() {
        for(;;) {
            uint64_t from = atomic_load(nodes->id);
            id_type to = {from};
            if(!to.next)
                return nullptr;
            uint16_t nn = to.next;
            uint64_t next = atomic_load((nodes + nn)->id);
            if constexpr(blist)
                if(next >= locks_from)
                    continue;
            id_type n = {next};
            id_type next_next_to;
            if constexpr(blist) {
                if(!n.next)
                    to.prev = 0;
                else {
                    next_next_to.id = atomic_load((nodes + n.next)->id);
                    if(next_next_to.prev != nn || next_next_to.id >= locks_from)
                        continue;
                    if(!atomic_compare_exchange((nodes + n.next)->id, next_next_to.id, pop_lock))
                        continue;
                }
            }
            to.next = n.next;
            ++to.salt;
            bool succ = (atomic_compare_exchange(nodes->id, from, to.id));
            if constexpr(blist) {
                if(to.next) {
                    if(succ)
                        next_next_to.prev = uint16_t();
                    atomic_store((nodes + n.next)->id, next_next_to.id);
                }
            }
            if(succ)
                return to_type(nodes + nn);
        }
    }
    void push(type* p) {
        if constexpr(blist)
            push_back(p);
        else
            push_front(p);
    }
    type* pop() {
        return pop_front();
    }
    static constexpr uint32_t run_once() {
        return 0;
    }

    template<typename node>
    struct iter : list_iterator<node, blist>
    {
        node* nodes;

        iter(node* nodes) : list_iterator<node, blist>(nodes, true), nodes(nodes) {
        }
        iter(node* nodes, node* n) : list_iterator<node, blist>(n), nodes(nodes) {
        }
        iter& operator++() {
            if(!this->n->next)
                this->n = nullptr;
            else
                this->n = nodes + this->n->next;
            return *this;
        }
        iter& operator--() requires(blist) {
            this->n = nodes + this->n->prev;
            return *this;
        }
        auto& operator*() {
            return *((type*)this->n->value_buf);
        }
        auto* operator->() {
            return (type*)this->n->value_buf;
        }
    };

    typedef iter<const node> const_iterator;
    typedef iter<node> iterator;

    iterator begin() {
        return iterator(nodes, nodes + nodes->next);
    }
    const_iterator begin() const {
        return const_iterator(nodes, nodes + nodes->next);
    }
    iterator end() {
        return iterator(nodes);
    }
    const_iterator end() const {
        return const_iterator(nodes);
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

        uint32_t pre_alloc = 0;
        uint32_t max_size = 0;

        bool use_malloc = false;

        int cont_type = 2; //tss_pvector 0, forward_list 1, blist 2
        int allocator_cont = 1;

        const constexpr fast_alloc_params operator()(uint32_t pre_alloc, uint32_t max_size = 0) const {
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc, cont_type, allocator_cont);
        }
        const constexpr fast_alloc_params alloc_params() const {
            if(use_mt && use_tss)
                return fast_alloc_params(false, true, pre_alloc, max_size, use_malloc, cont_type, allocator_cont);
            else
                return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc, cont_type, allocator_cont);
        }
        const constexpr fast_alloc_params malloc(bool use_malloc = true) const {
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc, cont_type, allocator_cont);
        }
        const constexpr fast_alloc_params operator()(container_tag cont, container_tag ac = forward_list_t) const {
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc, cont.value, ac.value);
        }
        constexpr int node_type() const {
            return max(cont_type, allocator_cont);
        }
    };

    static constexpr fast_alloc_params st({false, false});
    static constexpr fast_alloc_params mt({true, false});
    static constexpr fast_alloc_params mt_tss({true, true});
    static constexpr fast_alloc_params st_tss({false, true}); //for allocator
}

using namespace alloc_params;

//#define ENABLE_ALLOC_FREE_PROFILE

template<typename type, fast_alloc_params params = st_tss,
    template<typename, bool, bool, bool, typename> typename nodes_type_ = forward_list, typename node_type = forward_list_node<type> >
struct fast_alloc
{
    static const bool use_mt = params.use_mt;
    static const bool use_tss = params.use_tss;
    static const uint32_t pre_alloc = use_tss ? 0 : params.pre_alloc;
    static const uint32_t max_size = use_tss ? 0 : params.max_size;

    typedef nodes_type_<type, use_mt, use_tss, true, node_type> nodes_type;
    nodes_type nodes;
    typedef nodes_type::node node;
    uint32_t size;

    fast_alloc() : size() {
        for(uint32_t i = 0; i != pre_alloc; ++i)
            nodes.push(nodes_type::to_type(new node));
        if constexpr(max_size)
            size += pre_alloc;
    }
    type* alloc() {
#ifdef ENABLE_ALLOC_FREE_PROFILE
        MPROFILE("fast_alloc::alloc")
#endif

        type* p = nodes.pop();
        if(p)
        {
            //MPROFILE("fast_alloc::alloc() success")
        }
        if(!p) {
            MPROFILE("fast_alloc::alloc() slow")
            return nodes_type::to_type(new node);
        }
        else if constexpr(max_size)
            atomic_sub(size, 1);
        return p;
    }
    void free(type* p) {
#ifdef ENABLE_ALLOC_FREE_PROFILE
        MPROFILE("fast_alloc::free")
#endif

        if constexpr(max_size)
        {
            uint32_t size_ = atomic_load(size);
            if(size_ < max_size) {
                nodes.push(p);
                atomic_add(size, 1);
            }
            else {
                MPROFILE("fast_alloc::free() slow")
                delete nodes_type::to_node(p);
                return;
            }
        }
        else
            nodes.push(p);
    }
    uint32_t run_once() {
        if constexpr(max_size == 0)
            return 0;
        MPROFILE("fast_alloc::run_once()")
        uint32_t ret = 0;
        while(atomic_load(size) > max_size)
        {
            MPROFILE("fast_alloc::run_once() pop")
            type* p = nodes.pop();
            if(p) {
                atomic_sub(size, 1);
                delete nodes_type::to_node(p);
                ++ret;
            }
            else {
                MPROFILE("fast_alloc::run_once() pop race")
                return ret;
            }
        }
        while(atomic_load(size) < pre_alloc)
        {
            MPROFILE("fast_alloc::run_once() push")
            node* n = new node;
            nodes.push(nodes_type::to_type(n));
            atomic_add(size, 1);
            ++ret;
        }
        return ret;
    }
};

template<typename type, typename node_type>
struct malloc_alloc
{
    type* alloc() {

#ifdef ENABLE_ALLOC_FREE_PROFILE
        MPROFILE("malloc_alloc::alloc")
#endif

        node_type* n = (node_type*)::malloc(sizeof(node_type));
        if(!n)
            throw std::bad_alloc();
        return (type*)n;
    }
    void free(type* n) {

#ifdef ENABLE_ALLOC_FREE_PROFILE
        MPROFILE("malloc_alloc::free")
#endif

        ::free((node_type*)n);
    }
    static constexpr uint32_t run_once() {
        return 0;
    }
};

template<int index, int index_from, typename ... params>
struct conditional_multi_f;

template<int index, int index_from, typename param>
struct conditional_multi_f<index, index_from, param>
{
    typedef std::conditional_t<index == index_from, param, void> type;
};

template<int index, int index_from, typename param, typename ... params>
struct conditional_multi_f<index, index_from, param, params...>
{
    typedef std::conditional_t<index == index_from, param,
        typename conditional_multi_f<index, index_from + 1, params...>::type> type;
};

template<int index, typename param, typename ... params>
struct conditional_multi : conditional_multi_f<index, 0, param, params...>
{
};

template<int index, typename param, typename ... params>
using conditional_multi_t = typename conditional_multi<index, param, params...>::type;

template<typename type, int t>
struct node_type
{
    typedef conditional_multi_t<t, type, forward_list_node<type>, blist_node<type> > node;
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
    std::conditional_t<params.use_malloc, malloc_alloc<type_, node> ,
        conditional_multi_t<params.allocator_cont,
            fast_alloc<type_, params.alloc_params(), tss_pvector, node>,
            fast_alloc<type_, params.alloc_params(), forward_list, node>,
            fast_alloc<type_, params.alloc_params(), blist, node>
    > > type;
};

template<typename type, fast_alloc_params params = mt_tss, typename node = node_type<type, params.node_type()>::node>
struct fast_alloc_list :
    allocator_type<type, node, params>::type,
    container_type<type, node, params>::type,
    emplace_decl<type, fast_alloc_list<type, params, node> >
{
    fast_alloc_list() {
    }
    fast_alloc_list(str_holder) {
    }
};

