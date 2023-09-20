/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#include "math.hpp"

#include <cmath>

namespace m
{ 
    double sqrt(double v)
    {
        return ::sqrt(v);
    }

    double modf(double x, double *iptr)
    {
        return ::modf(x, iptr);
    }

    double abs(double x)
    {
        return std::abs(x);
    }
}

