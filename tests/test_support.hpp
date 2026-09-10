#pragma once

#include <cstdio>
#include <cstdlib>

#define CHECK(condition)                                                                          \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            std::abort();                                                                         \
        }                                                                                         \
    } while (false)
