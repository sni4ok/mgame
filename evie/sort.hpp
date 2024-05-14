#pragma once

#include "algorithm.hpp"

template<typename type, typename comp>
int qsort_cmp(const void* p1, const void* p2, void* cmp)
{
    const type& l = *(const type*)p1;
    const type& r = *(const type*)p2;
    comp& c = *((comp*)cmp);
    if(c(l, r))
        return -1;
    if(c(r, l))
        return 1;
    return 0;
}

extern "C"
{
    typedef int (*__compar_d_fn_t) (const void *, const void *, void *);
    extern void qsort_r (void *, size_t, size_t, __compar_d_fn_t, void *) __nonnull ((1, 4));
}

template<typename type, typename comp = less<type> >
void sort(type* from, type* to, comp cmp = comp())
{
    qsort_r(from, to - from, sizeof(type), qsort_cmp<type, comp>, &cmp);
}

