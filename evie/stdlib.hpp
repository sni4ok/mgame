/*
    author: Ilya Andronov <sni4ok@yandex.ru>
*/

#pragma once

#include "stdint.hpp"

extern "C"
{
#define NE noexcept
#define NN(...) __attribute__ ((__nonnull__  (__VA_ARGS__) ))
#define PURE __attribute__ ((__pure__))
#define UR __attribute__ ((__warn_unused_result__))
#define AM __attribute__ ((__malloc__))
#define AS(...) __attribute__ ((__alloc_size__  (__VA_ARGS__) ))

    extern void* memset(void*, int, size_t) NE NN(1);
    extern void* memcpy(void*, const void*, size_t) NE NN(1, 2);
    extern void* memmove(void*, const void*, size_t) NE NN(1, 2);
    extern int memcmp(const void*, const void*, size_t) NE PURE NN(1, 2);
    extern int strcmp(char_cit, char_cit) NE PURE NN(1, 2);
    extern size_t strnlen(char_cit, size_t) NE PURE NN(1);
    extern void* malloc (size_t) NE AM AS(1) UR;
    extern void* realloc(void*, size_t) NE UR AS(2);
    extern void* calloc (size_t, size_t) NE AM AS(1,2) UR;
    extern void free(void*) NE;
    extern char_it getenv(char_cit) NE NN(1) UR;
    extern int usleep(u32);
    extern int close(int);
    extern int system(char_cit);
    extern char_it strdup(char_cit) NE AM NN(1);

#undef NE
#undef NN
#undef PURE
#undef UR
#undef AM
#undef AS
}

char_cit find(char_cit from, char_cit to, char c);
char_it find(char_it from, char_it to, char c);

