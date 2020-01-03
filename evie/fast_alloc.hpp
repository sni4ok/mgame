/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "profiler.hpp"

#include <atomic>

template<typename type, uint32_t size>
struct array
{
    type elems[size];
    type& operator[](uint32_t idx) {
        return elems[idx];
    }
    const type& operator[](uint32_t idx) const {
        return elems[idx];
    }
};

//with follower can work one writer and many readers without synchronization
template<typename type, uint32_t block_size = 16384, uint32_t num_blocks = 16384>
class follower : noncopyable
{
    typedef array<type*, num_blocks> bulks_type;
    bulks_type bulks;
    std::atomic<uint32_t> num_elems;
public:
    typedef type value_type;

    follower() : num_elems() {
    }
    ~follower() {
        uint32_t elems = num_elems;
        uint32_t end_bulk = elems / block_size;
        uint32_t end_element = elems % block_size;
        if(end_element)
            ++end_bulk;
        for(uint32_t i = 0; i != end_bulk; ++i)
            delete[] bulks[i];
    }
    void push_back(const type& value) {
        uint32_t elems = num_elems;
        uint32_t end_bulk = elems / block_size;
        uint32_t end_element = elems % block_size;

        if(!end_element) {
            MPROFILE("follower::new")
            bulks[end_bulk] = new type[block_size];
        }

        (bulks[end_bulk])[end_element] = value;
        ++num_elems;
    }
    class const_iterator :
        public std::iterator<std::random_access_iterator_tag, type, int32_t>
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
        return const_iterator(bulks, num_elems);
    }
    const type& operator[](uint32_t idx) const {
        return *(begin() + idx);
    }
    uint32_t size() const {
        return num_elems;
    }
};

template<typename type, uint32_t capacity>
class lockfree_queue : noncopyable
{
    struct node
    {
        type elem;
        //0 - empty and free
        //1 - push lock
        //2 - filled and free
        //3 - pop lock
        std::atomic<uint32_t> status;
    };
    array<node, capacity> elems;

    std::atomic<uint64_t> push_cnt, pop_cnt;
    static uint64_t get_idx(uint64_t idx) {
        return idx % capacity;
    }
public:
    void push(const type& t) {
        push(type(t));
    }
    void push(type&& t) {
        node& n = elems[get_idx(push_cnt++)];
        uint32_t status = 0;
        if(!n.status.compare_exchange_strong(status, 1))
            throw std::runtime_error("lockfree_queue overloaded");
        n.elem = std::move(t);
        status = 1;
        if(!n.status.compare_exchange_strong(status, 2))
            throw std::runtime_error("lockfree_queue internal push error");
    }
    void pop_strong(type& t) {
        node& n = elems[get_idx(pop_cnt++)];
        uint32_t status = 2;
        if(!n.status.compare_exchange_strong(status, 3))
            throw std::runtime_error("lockfree_queue::pop_strong() lock error");
        t = std::move(n.elem);
        status = 3;
        if(!n.status.compare_exchange_strong(status, 0))
            throw std::runtime_error("lockfree_queue::pop_strong() internal error");
    }
    bool pop_weak(type& t) {
        node& n = elems[get_idx(pop_cnt)];
        uint32_t status = 2;
        if(n.status.compare_exchange_strong(status, 3)) {
            ++pop_cnt;
            t = std::move(n.elem);
            status = 3;
            if(!n.status.compare_exchange_strong(status, 0))
                throw std::runtime_error("lockfree_queue internal pop error");
            return true;
        }
        return false;
    }
};

inline void follower_test()
{
    follower<int> deq2;
    follower<int> deq;
    deq.push_back(10);
    follower<int>::const_iterator i1 = deq.begin(), i2 = deq.end();
    deq.push_back(15);
    follower<int>::const_iterator i3 = deq.end();
    deq.push_back(35);
    deq.push_back(45);
    deq.push_back(25);
    for(; i1 != i2; ++i1){
        *i1;
    }
    for(; i1 != i3; ++i1){
        *i1;
    }

    follower<int>::const_iterator i4 = i3 - 2;
    *i4;
}
 
template<typename ttype, uint32_t pool_size = 16 * 1024>
struct fast_alloc
{
    typedef ttype type;
    lockfree_queue<type*, pool_size> elems;

    fast_alloc()
    {
        for(uint32_t i = 0; i != pool_size; ++i)
            elems.push(new type);
    }
    type* alloc()
    {
        type* p;
        elems.pop_strong(p);
        return p;
    }
    void free(type* m)
    {
        elems.push(m);
    }
    ~fast_alloc()
    {
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
    alloc_holder(alloc& a) : a(a), p(a.alloc())
    {
    }
    ~alloc_holder()
    {
        if(p)
            a.free(p);
    }
    type* operator->()
    {
        return p;
    }
    type& operator*()
    {
        return *p;
    }
    type* release()
    {
        type* ret = p;
        p = nullptr;
        return ret;
    }
};


