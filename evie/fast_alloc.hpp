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

struct fast_alloc_params
{
    bool use_mt;
    bool use_tss;

    uint32_t pre_alloc = 0;
    uint32_t max_size = 0;

    bool use_malloc = false;

    const constexpr fast_alloc_params operator()(uint32_t pre_alloc, uint32_t max_size = 0) const {
        return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc);
    }
    const constexpr fast_alloc_params alloc_params() const {
        if(use_mt && use_tss)
            return fast_alloc_params(false, true, pre_alloc, max_size, use_malloc);
        else
            return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc);
    }
    const constexpr fast_alloc_params malloc(bool use_malloc = true) const {
        return fast_alloc_params(use_mt, use_tss, pre_alloc, max_size, use_malloc);
    }
};

static constexpr fast_alloc_params st({false, false});
static constexpr fast_alloc_params mt({true, false});
static constexpr fast_alloc_params mt_tss({true, true});
static constexpr fast_alloc_params st_tss({false, true}); //for allocator

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

template<typename type, fast_alloc_params params, template<typename, bool, bool, bool, typename> typename nodes_type, typename node_type>
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

template<typename type, fast_alloc_params params = mt_tss,
    template<typename, bool, bool, bool, typename> typename cont = blist,
    template<typename, bool, bool, bool, typename> typename allocator_cont = forward_list>
struct fast_alloc_list :
    std::conditional<params.use_malloc, malloc_alloc<type, params.alloc_params(), allocator_cont, blist_node<type> > ,
                                        fast_alloc<type, params.alloc_params(), allocator_cont, blist_node<type> > >::type,
    cont<type, params.use_mt, false, true, blist_node<type> >
{
    fast_alloc_list() {
    }
    fast_alloc_list(str_holder) {
    }
    template<typename ... par>
    void emplace(par&& ... args)
    {
        type* p = this->alloc();
        new(p)type(args...);
        this->push(p);
    }
    template<typename ... par>
    void emplace_front(par&& ... args)
    {
        type* p = this->alloc();
        new(p)type(args...);
        this->push_front(p);
    }
    template<typename ... par>
    void emplace_back(par&& ... args)
    {
        type* p = this->alloc();
        new(p)type(args...);
        this->push_front(p);
    }
};

