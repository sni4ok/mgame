#include "tuple.hpp"
#include "math.hpp"
#include "optional.hpp"
#include "profiler.hpp"

template<typename t>
struct bit
{
    typedef t type;
    static const u64 elems = sizeof(type) * 8;

    type value;

    static constexpr u64 key(type v)
    {
        return v;
    }
    static constexpr type bits(u64 k)
    {
        return k;
    }
    static inline int clz(type v)
    {
        return __builtin_clzg(v);
    }
    static inline int ctz(type v)
    {
        return __builtin_ctzg(v);
    }
    static inline int count(type v)
    {
        return __builtin_popcountg(v);
    }
    type first_bit() const
    {
        assert(value);
        return ctz(value);
    }
    type last() const
    {
        assert(value);
        int bit = clz(value);
        return type(elems - bit - 1);
    }
    optional<type> begin() const
    {
        if(!value)
            return none;
        return type(ctz(value));
    }
    type next_bit(type tp) const
    {
        if(tp + 1 == elems)
            return type();

        type v = (value >> (tp + 1)) << (tp + 1);
        if(!v)
            return type();

        return bit(v).first_bit();
    }
    optional<type> prev_bit(type tp) const
    {
        u64 v = value >> tp << tp;
        v = v ^ value;
        if(!v)
            return none;

        return bit(v).last();
    }
    operator type() const
    {
        return value;
    }
    bool is_set(type bit) const
    {
        return value & (type(1) << bit);
    }
    void set(type bit)
    {
        value = value | (type(1) << bit);
    }
    bool unset(type bit)
    {
        value = value & ~(type(1) << bit);
        return !value;
    }
    bool empty() const
    {
        return !value;
    }
    u64 size() const
    {
        return count(value);
    }
};

#ifdef USE_INT128_EXT
    typedef bit<u128> bit128;
#endif

typedef bit<u64> bit64;
typedef bit<u32> bit32;

template<typename ... bits>
struct idx_bits;

template<typename bit>
struct idx_bits<bit> : bit
{
    typedef typename bit::type bits_t;

    bool next(bits_t& b) const
    {
        typename bit::type n = this->next_bit(b);
        if(n)
        {
            b = n;
            return true;
        }
        return false;
    }
    bool prev(bits_t& b) const
    {
        auto p = this->prev_bit(b);
        if(!!p)
        {
            b = *p;
            return true;
        }
        return false;
    }
};

template<typename bit0, typename bit1>
struct idx_bits<bit0, bit1>
{
    typedef tuple<typename bit0::type, typename bit1::type> bits_t;
    static const u64 elems = bit0::elems * bit1::elems;

    bit0 idx0 = bit0();
    bit1 idx1[bit0::elems] = {};

    idx_bits() = default;
    idx_bits(const idx_bits&) = delete;

#define NEXT(next, first, op) \
        auto& [b0, b1] = bits; \
        auto n1 = idx1[b0].next(b1); \
        if(!!n1) \
        { \
            b1 = op n1; \
            return true; \
        } \
        auto n0 = idx0.next(b0); \
        if(!!n0) \
        { \
            b0 = op n0; \
            b1 = idx1[b0].first(); \
            return true;\
        } \
        return false;

    bool next(bits_t& bits) const
    {
        NEXT(next_bit, first_bit, )
    }
    bool prev(bits_t& bits) const
    {
        NEXT(prev_bit, last, *)
    }
#undef NEXT

    static constexpr u64 key(const bits_t& bits)
    {
        const auto& [b0, b1] = bits;
        return b0 * bit1::elems + b1;
    }
    static constexpr bits_t bits(u64 k)
    {
        bits_t b{k / bit1::elems, k % bit1::elems};
        assert(get<0>(b) <= bit0::elems && get<1>(b) <= bit1::elems);
        return b;
    }
    optional<bits_t> begin() const
    {
        optional<typename bit0::type> b0 = idx0.begin();
        if(!b0)
            return none;
        return bits_t{*b0, idx1[*b0].first_bit()};
    }
    bits_t last() const
    {
        typename bit0::type b0 = idx0.last();
        return bits_t{b0, idx1[b0].last()};
    }
    bool is_set(const bits_t& bits) const
    {
        const auto& [b0, b1] = bits;
        return idx1[b0].is_set(b1);
    }
    void set(const bits_t& bits)
    {
        const auto& [b0, b1] = bits;
        idx0.set(b0);
        idx1[b0].set(b1);
    }
    bool unset(const bits_t& bits)
    {
        const auto& [b0, b1] = bits;
        if(idx1[b0].unset(b1))
            return idx0.unset(b0);
        return false;
    }
    bool empty() const
    {
        return idx0.empty();
    }
    u64 size() const
    {
        u64 sz0 = idx0.size();
        if(!sz0)
            return sz0;

        typename bit0::type b0 = idx0.first_bit();
        u64 sz = 0, i = 0;

        for(;;)
        {
            sz += idx1[b0].size();
            ++i;
            if(i == sz0)
                break;

            b0 = idx0.next_bit(b0);
            assert(b0);
        }

        return sz;
    }
};

template<typename bit0, typename bit1, typename bit2>
struct idx_bits<bit0, bit1, bit2>
{
    typedef tuple<typename bit0::type, typename bit1::type, typename bit2::type> bits_t;
    static const u64 elems = bit0::elems * bit1::elems * bit2::elems;

    bit0 idx0 = bit0();
    bit1 idx1[bit0::elems] = {};
    bit2 idx2[bit0::elems * bit1::elems] = {};

    idx_bits() = default;
    idx_bits(const idx_bits&) = delete;

#define NEXT(next, first, op) \
        auto& [b0, b1, b2] = bits; \
        auto n2 = idx2[b0 * bit1::elems + b1].next(b2); \
        if(!!n2) \
        { \
            b2 = op n2; \
            return true; \
        } \
        auto n1 = idx1[b0].next(b1); \
        if(!!n1) \
        { \
            b1 = op n1; \
            b2 = idx2[b0 * bit1::elems + b1].first(); \
            return true; \
        } \
        auto n0 = idx0.next(b0); \
        if(!!n0) \
        { \
            b0 = op n0; \
            b1 = idx1[b0].first(); \
            b2 = idx2[b0 * bit1::elems + b1].first(); \
            return true; \
        } \
        return false;

    bool next(bits_t& bits) const
    {
        NEXT(next_bit, first_bit, )
    }
    bool prev(bits_t& bits) const
    {
        NEXT(prev_bit, last, *)
    }
#undef NEXT

    static constexpr u64 key(const bits_t& bits)
    {
        const auto& [b0, b1, b2] = bits;
        return b0 * bit1::elems * bit2::elems + b1 * bit2::elems + b2;
    }
    static constexpr bits_t bits(u64 k)
    {
        bits_t b{k / (bit1::elems * bit2::elems), (k / bit2::elems) % bit1::elems, k % bit2::elems};
        assert(get<0>(b) <= bit0::elems && get<1>(b) <= bit1::elems && get<2>(b) <= bit2::elems);
        return b;
    }
    optional<bits_t> begin() const
    {
        optional<typename bit0::type> b0 = idx0.begin();
        if(!b0)
            return none;
        typename bit1::type b1 = idx1[*b0].first_bit();
        return bits_t{*b0, b1, idx2[*b0 * bit1::elems + b1].first_bit()};
    }
    bits_t last() const
    {
        typename bit0::type b0 = idx0.last();
        typename bit1::type b1 = idx1[b0].last();
        return bits_t{b0, b1, idx2[b0 * bit1::elems + b1].last()};
    }
    bool is_set(const bits_t& bits) const
    {
        const auto& [b0, b1, b2] = bits;
        return idx2[b0 * bit1::elems + b1].is_set(b2);
    }
    void set(const bits_t& bits)
    {
        const auto& [b0, b1, b2] = bits;
        idx0.set(b0);
        idx1[b0].set(b1);
        idx2[b0 * bit1::elems + b1].set(b2);
    }
    bool unset(const bits_t& bits)
    {
        const auto& [b0, b1, b2] = bits;
        if(idx2[b0 * bit1::elems + b1].unset(b2))
        {
            if(idx1[b0].unset(b1))
                return idx0.unset(b0);
        }
        return false;
    }
    bool empty() const
    {
        return idx0.empty();
    }
    u64 size() const
    {
        u64 sz0 = idx0.size();
        if(!sz0)
            return sz0;

        typename bit0::type b0 = idx0.first_bit();
        u64 sz = 0, i = 0;

        for(;;)
        {
            const bit1& id1 = idx1[b0];
            typename bit0::type b1 = id1.first_bit();
            for(;;)
            {
                sz += idx2[b0 * bit1::elems + b1].size();
                b1 = id1.next_bit(b1);
                if(!b1)
                    break;
            }

            ++i;
            if(i == sz0)
                break;

            b0 = idx0.next_bit(b0);
            assert(b0);
        }
        return sz;
    }
};

typedef idx_bits<bit64, bit64, bit64> ib3;
typedef idx_bits<bit64, bit64> ib2;
typedef idx_bits<bit64> ib1;

template<typename key, typename value, bool less = true, u32 price_cvt = 1000
    , bool shift_key = false, typename idx_root = ib1, typename idx_node = ib3
    >
struct price_map
{
    typedef ::pair<key, value> pair;
    typedef key key_type;
    typedef value value_type;
    static const u64 elems = idx_root::elems * idx_node::elems;

    struct node : idx_node
    {
        pair values[idx_node::elems] = {};
    };

    idx_root root_idx = idx_root();
    node* nodes[idx_root::elems] = {};

    price_map()
    {
    }
    ~price_map()
    {
        clear();
    }
    void alloc(node*& ptr)
    {
        MPROFILE("price_map::alloc")
        ptr = (node*)malloc(sizeof(node));
        memset((void*)ptr, 0, sizeof(idx_node));
        if(!ptr) [[unlikely]]
            throw str_exception("price_map allocation error");
    }
    void free(node*& ptr)
    {
        ::free(ptr);
        ptr = nullptr;
    }

    price_map(const price_map&) = delete;

    static constexpr ::pair<u64, u64> keys(key k)
    {
        if constexpr(price_cvt)
        {
            assert(!(k.value % price_cvt));
            k.value = k.value / price_cvt;
        }

        if constexpr(shift_key)
            k.value = k.value % (idx_root::elems * idx_node::elems);

        if constexpr(!less)
            k.value = idx_root::elems * idx_node::elems - k.value;

        assert(k.value >= 0);
        ::pair<u64, u64> r(k.value / idx_node::elems, k.value % idx_node::elems);
        assert(r.first <= idx_root::elems);
        return r;
    }
    static constexpr ::pair<typename idx_root::bits_t, typename idx_node::bits_t> bits(u64 k1, u64 k2)
    {
        return {idx_root::bits(k1), idx_node::bits(k2)};
    }
    static constexpr tuple<u64, u64, typename idx_root::bits_t, typename idx_node::bits_t> keys_bits(key k)
    {
        auto [k1, k2] = keys(k);
        auto [b1, b2] = bits(k1, k2);
        return {k1, k2, b1, b2};
    }
    struct iterator
    {
        typedef bidirectional_iterator_tag iterator_category;

        pair* v = nullptr;
        price_map* map;

        bool operator==(const iterator& r) const
        {
            return v == r.v;
        }
        iterator& set_k2(node* ptr, const idx_node::bits_t& b2)
        {
            u64 k2 = idx_node::key(b2);
            pair& p = ptr->values[k2];
            v = &p;
            return *this;
        }
        iterator& operator++()
        {
            auto [k1, k2, b1, b2] = keys_bits(v->first);
            node* ptr = map->nodes[k1];

            if(ptr->next(b2))
                return set_k2(ptr, b2);

            if(map->root_idx.next(b1))
            {
                k1 = idx_root::key(b1);
                ptr = map->nodes[k1];
                return set_k2(ptr, *ptr->begin());
            }

            v = nullptr;
            return *this;
        }
        iterator& operator--()
        {
            if(!v)
            {
                v = const_cast<pair*>(&map->back());
                return *this;
            }

            auto [k1, k2, b1, b2] = keys_bits(v->first);
            node* ptr = map->nodes[k1];

            if(ptr->prev(b2))
                return set_k2(ptr, b2);

            if(map->root_idx.prev(b1)) [[likely]]
            {
                k1 = idx_root::key(b1);
                ptr = map->nodes[k1];
                return set_k2(ptr, ptr->last());
            }

            throw str_exception("price_map::iterator decrement begin()");
        }
        pair* operator->()
        {
            return v;
        }
        const pair* operator->() const
        {
            return v;
        }
        pair& operator*()
        {
            return *v;
        }
        const pair& operator*() const
        {
            return *v;
        }
    };

    ::pair<iterator, bool> __insert(const key& k, bool fe)
    {
        //MPROFILE("price_map::insert");
        auto [k1, k2, b1, b2] = keys_bits(k);
        node*& ptr = nodes[k1];
        bool have_value;

        if(!root_idx.is_set(b1))
        {
            if(!ptr)
                alloc(ptr);

            root_idx.set(b1);
            have_value = false;
        }
        else
        {
            assert(ptr);
            have_value = ptr->is_set(b2);
        }

        pair& v = ptr->values[k2];

        if(!have_value)
        {
            v.first = k;
            if(fe)
                memset(&v.second, 0, sizeof(v.second));
            ptr->set(b2);
        }
        return {{&v, this}, !have_value};
    }
    ::pair<iterator, bool> insert(const key& k)
    {
        return __insert(k, true);
    }
    ::pair<iterator, bool> insert(const ::pair<key, value>& v)
    {
        auto it = __insert(v.first, false);
        it.first.v->second = v.second;
        return insert(v.first);
    }
    value& operator[](const key& k)
    {
        auto v = insert(k);
        return v.first.v->second;
    }
    iterator find(const key& k)
    {
        auto [k1, k2] = keys(k);
        auto b1 = idx_root::bits(k1);

        if(!root_idx.is_set(b1))
            return end();

        auto b2 = idx_node::bits(k2);
        node*& ptr = nodes[k1];

        if(!ptr->is_set(b2))
            return end();

        return {&ptr->values[k2], this};
    }
    iterator lower_bound(const key& k)
    {
        auto [k1, k2] = keys(k);
        auto b1 = idx_root::bits(k1);

        if(!root_idx.is_set(b1))
        {
            if(!root_idx.next(b1))
                return end();
            k1 = idx_root::key(b1);
            k2 = 0;
        }

        auto b2 = idx_node::bits(k2);
        node*& ptr = nodes[k1];

        if(!ptr->is_set(b2))
        {
            if(!ptr->next(b2))
            {
                if(!root_idx.next(b1))
                    return end();
                k1 = idx_root::key(b1);
                node*& ptr = nodes[k1];
                b2 = *ptr->begin();
            }
            k2 = idx_node::key(b2);
        }

        return {&ptr->values[k2], this};
    }
    iterator end()
    {
        return {nullptr, this};
    }
    iterator begin()
    {
        auto b1 = root_idx.begin();
        if(!b1)
            return end();

        u64 k1 = idx_root::key(*b1);
        node* ptr = nodes[k1];
        auto b2 = ptr->begin();
        assert(!!b2);
        u64 k2 = idx_node::key(*b2);
        pair& v = ptr->values[k2];
        return {&v, this};
    }
    void erase(iterator it)
    {
        assert(it.v);
        auto [k1, k2] = keys(it.v->first);
        node* ptr = nodes[k1];
        auto b2 = idx_node::bits(k2);
        if(ptr->unset(b2))
        {
            auto b1 = idx_root::bits(k1);
            root_idx.unset(b1);
        }
    }
    bool erase(const key& k)
    {
        auto it = find(k);
        if(it != end())
        {
            erase(it);
            return true;
        }
        return false;
    }
    bool empty() const
    {
        return root_idx.empty();
    }
    u64 size() const
    {
        auto b1o = root_idx.begin();
        if(!b1o)
            return 0;

        typename idx_root::bits_t& b1 = *b1o;
        u64 sz = 0;
        node* ptr = nullptr;

    rep:
        ptr = nodes[idx_root::key(b1)];
        sz += ptr->size();
        if(root_idx.next(b1))
            goto rep;

        return sz;
    }
    void clear()
    {
        //MPROFILE("price_map::clear");
        root_idx = idx_root();
        for(node*& ptr: nodes)
            free(ptr);
    }
    const pair& back() const
    {
        assert(!empty());
        auto b1 = root_idx.last();
        node* ptr = nodes[idx_root::key(b1)];
        auto b2 = ptr->last();
        return ptr->values[idx_node::key(b2)];
    }
};

