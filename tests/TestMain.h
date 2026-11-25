// Tiny assert framework for the coop test suite (no external deps).
#pragma once

#include <cstdio>
#include <cstring>
#include <string>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) do { \
    ++g_checks; \
    if (!(cond)) { \
        ++g_failures; \
        std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_EQ(a, b) do { \
    ++g_checks; \
    if (!((a) == (b))) { \
        ++g_failures; \
        std::printf("FAIL %s:%d  %s != %s\n", __FILE__, __LINE__, #a, #b); \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    ++g_checks; \
    double _d = double(a) - double(b); if (_d < 0) _d = -_d; \
    if (!(_d <= (eps))) { \
        ++g_failures; \
        std::printf("FAIL %s:%d  |%s - %s| > %s\n", __FILE__, __LINE__, #a, #b, #eps); \
    } \
} while (0)

inline int TestReport(const char* suite) {
    std::printf("\n%s: %d checks, %d failures\n", suite, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
