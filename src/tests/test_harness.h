#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include "utils.h"
#include <stdio.h>

extern int global_tests_run;
extern int global_tests_failed;

#define test_assert_m(cond) do {                                        \
    global_tests_run++;                                                 \
    if(!(cond)) {                                                       \
        global_tests_failed++;                                          \
        printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                   \
} while(0)

#define test_require_m(cond) do {                                       \
    global_tests_run++;                                                 \
    if(!(cond)) {                                                       \
        global_tests_failed++;                                          \
        printf("  FAIL %s:%d %s (required, test aborted)\n",            \
            __FILE__, __LINE__, #cond);                                 \
        return;                                                         \
    }                                                                   \
} while(0)

#define test_assert_eq_u64_m(a, b) do {                                 \
    global_tests_run++;                                                 \
    u64 _a = (a), _b = (b);                                             \
    if(_a != _b) {                                                      \
        global_tests_failed++;                                          \
        printf("  FAIL %s:%d  %s == %s (%llu != %llu)\n",               \
            __FILE__, __LINE__, #a, #b, _a, _b);                        \
    }                                                                   \
} while(0)

#endif