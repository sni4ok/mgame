/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "profiler.hpp"
#include "string.hpp"
#include "atomic.hpp"

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

    void throw_exception(const char* reason, uint32_t status)
    {
        throw mexception(es() % name % ": lockfree_queue: " % _str_holder(reason) % ", (" % capacity % "," % uint64_t(pop_cnt) % ","
            % uint64_t(push_cnt) % "," % status % ")");
    }

    lockfree_queue(const lockfree_queue&) = delete;
public:
    lockfree_queue(str_holder name) : name(name), push_cnt(), pop_cnt() {
    }
    static const constexpr uint32_t capacity = capacity_size;

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

template<typename type, uint32_t capacity_size>
struct st_queue
{
    type elems[capacity_size];
    uint64_t push_cnt, pop_cnt;

    st_queue(str_holder) : elems() {
    }

    bool push_weak(type t) {
        push_cnt = push_cnt % capacity_size;
        type& v = elems[push_cnt];
        if(v)
            return false;
        ++push_cnt;
        v = t;
        return true;
    }

    void push(type t) {
        if(!push_weak(t))
        {
            MPROFILE("st_queue::push() overloaded")
            delete t;
        }
    }

    bool pop_weak(type& t) {
        pop_cnt = pop_cnt % capacity_size;
        type& v = elems[pop_cnt];
        if(!v)
            return false;
        ++pop_cnt;
        t = v;
        v = nullptr;
        return true;
    }
};

template<typename type_t, uint32_t pool_size = 16 * 1024, bool enable_run_once = true,
    template<typename, uint32_t> class queue = lockfree_queue>
struct fast_alloc
{
    typedef type_t type;
    queue<type_t*, pool_size> elems;
    int32_t free_elems;

    fast_alloc(str_holder name, uint32_t pre_alloc = 32) : elems(name), free_elems() {
        assert(pre_alloc <= pool_size);
        for(uint32_t i = 0; i != pre_alloc; ++i)
            elems.push(new type);
    }
    type* alloc() {
        type* p;
        if(!elems.pop_weak(p)) {
            MPROFILE("fast_alloc::alloc() slow")
            return new type;
        }
        else {
            if constexpr(enable_run_once)
                atomic_add(free_elems, 1);
            return p;
        }
    }
    void free(type* m) {
        if(!elems.push_weak(m)) {
            MPROFILE("fast_alloc::free() slow")
            delete m;
        }
        else {
            if constexpr(enable_run_once)
                atomic_sub(free_elems, 1);
        }
    }
    bool run_once() requires(enable_run_once) {
        int32_t c = atomic_load(free_elems);
        bool ret = false;
        for(int32_t i = 0; i < c; ++i) {
            type* p = new type;
            if(elems.push_weak(p)) {
                atomic_sub(free_elems, 1);
                ret = true;
            }
            else {
                delete p;
                break;
            }
        }
        return ret;
    }
    ~fast_alloc() {
        type* p;
        while(elems.pop_weak(p))
            delete p;
    }
};

template<typename alloc>
class alloc_holder
{
    alloc &a;
    typedef typename alloc::type type;
    type* p;

public:
    alloc_holder(alloc& a) : a(a), p(a.alloc()) {
    }
    ~alloc_holder() {
        if(p)
            a.free(p);
    }
    type* operator->() {
        return p;
    }
    type& operator*() {
        return *p;
    }
    type* release() {
        type* ret = p;
        p = nullptr;
        return ret;
    }
};

template<typename type>
struct lockfree_list
{
    struct node : type
    {
        node* next;
        node() : next(){}
    };

    lockfree_list() : tail(&root) {
    }
    void push(node* t) {
        for(;;)
        {
            node* expected = atomic_load(tail);
            if(atomic_compare_exchange(tail, expected, t))
            {
                expected->next = t;
                break;
            }
        }
    }
    node* begin() {
        return root.next;
    }
    node* next(node* prev) {
        if(!prev)
            return root.next;
        return prev->next;
    }

private:
    node root;
    node* tail;
};

