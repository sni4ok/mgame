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

template<typename type, uint32_t capacity_size>
class lockfree_queue
{
    struct node
    {
        type elem;
        //0 - empty and free
        //1 - push lock
        //2 - filled and free
        //3 - pop lock
        uint32_t status;
        node() : status(){}
    };

    str_holder name;
    node elems[capacity_size];
    uint64_t push_cnt, pop_cnt;

    static uint64_t get_idx(uint64_t idx) {
        return idx % capacity_size;
    }
    void throw_exception(const char* reason, uint32_t status) {
        throw mexception(es() % name % ": lockfree_queue: " % _str_holder(reason) % ", (" % capacity % "," % uint64_t(pop_cnt) % ","
            % uint64_t(push_cnt) % "," % status % ")");
    }

    lockfree_queue(const lockfree_queue&) = delete;
    static const constexpr uint32_t capacity = capacity_size;

public:
    lockfree_queue(str_holder name) : name(name), push_cnt(), pop_cnt() {
    }
    void push(const type& t) {
        push(type(t));
    }
    void push(type&& t) {
        uint32_t status = 0;
        node& n = elems[get_idx(atomic_add(push_cnt, 1) - 1)];
        while(!atomic_compare_exchange_r(n.status, status, 1)) {
            if(status != 3) {
                MPROFILE("lockfree_queue::push() overloaded")
                usleep(0);
            }
            status = 0;
        }
        n.elem = std::move(t);
        status = 1;
        if(!atomic_compare_exchange_r(n.status, status, 2))
            throw_exception("push() internal error", status);
    }
    bool push_weak(type t) {
        uint32_t status = 0;
        node& n = elems[get_idx(atomic_load(push_cnt))];
        if(atomic_compare_exchange_r(n.status, status, 1)) {
            atomic_add(push_cnt, 1);
            n.elem = t;
            status = 1;
            if(!atomic_compare_exchange_r(n.status, status, 2))
                throw_exception("push_weak() internal error", status);
            return true;
        }
        return false;
    }
    void pop_strong(type& t) {
        uint32_t status = 2;
        node& n = elems[get_idx(atomic_add(pop_cnt, 1) - 1)];
        if(!atomic_compare_exchange_r(n.status, status, 3))
            throw_exception("pop_strong() lock error", status);
        t = std::move(n.elem);
        status = 3;
        if(!atomic_compare_exchange_r(n.status, status, 0))
            throw_exception("pop_strong() internal error", status);
    }
    bool pop_weak(type& t) {
        for(;;) {
            uint32_t status = 2;
            node& n = elems[get_idx(atomic_load(pop_cnt))];
            if(atomic_compare_exchange_r(n.status, status, 3)) {
                atomic_add(pop_cnt, 1);
                t = std::move(n.elem);
                status = 3;
                if(!atomic_compare_exchange_r(n.status, status, 0))
                    throw_exception("pop_weak() internal error", status);
                return true;
            }
            else {
                if(status != 1 && status != 3)
                    return false;
            }
        }
    }
};

template<typename type, bool use_mt, bool use_tss, bool delete_on_exit, typename node_type = type>
struct lqueue
{
    typedef node_type node;

    lockfree_queue<type*, 10000> nodes;

    lqueue() : nodes("lqueue")
    {
    }

    static type* to_type(node* n) {
        return &n->value;
    }
    static node* to_node(type* p) {
        return (node*)p;
    }
    void push(type* p) {
        nodes.push(p);
    }
    type* pop() {
        type* p;
        nodes.pop_strong(p);
        return p;
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

