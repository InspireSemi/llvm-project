/**
 * @file test_common.h
 * @brief Unified test framework for unit and integration tests
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

// Test result types
typedef enum {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_SKIP = 2
} test_result_t;

// Test statistics
typedef struct {
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
    uint32_t skipped_tests;
} test_stats_t;

// Test function pointer
typedef test_result_t (*test_func_t)(void);

// Test case structure
typedef struct {
    const char* name;
    test_func_t test_func;
} test_case_t;

// Test suite structure
typedef struct {
    const char* name;
    const test_case_t* test_cases;
    uint32_t num_cases;
    void (*setup)(void);
    void (*teardown)(void);
} test_suite_t;

// Test framework functions
void test_framework_init(void);
test_stats_t test_get_stats(void);
void test_reset_stats(void);
uint32_t test_run_suite(const test_suite_t* suite);
uint32_t test_run_case(const test_case_t* test_case);
void test_print_summary(void);
const char* test_get_current_name(void);

// Enhanced assertion macros with detailed output
#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            printk("    FAIL: %s (line %d)\n", msg, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            printk("    FAIL: %s - expected %d, got %d (line %d)\n", \
                   msg, (int)(expected), (int)(actual), __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NEQ(unexpected, actual, msg) \
    do { \
        if ((unexpected) == (actual)) { \
            printk("    FAIL: %s - got unexpected value %d (line %d)\n", \
                   msg, (int)(actual), __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_PTR_EQ(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            printk("    FAIL: %s - expected %p, got %p (line %d)\n", \
                   msg, (void*)(expected), (void*)(actual), __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(expected, actual, msg) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printk("    FAIL: %s - expected '%s', got '%s' (line %d)\n", \
                   msg, (expected), (actual), __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_MEM_EQ(expected, actual, size, msg) \
    do { \
        if (memcmp((expected), (actual), (size)) != 0) { \
            printk("    FAIL: %s - memory differs at size %zu (line %d)\n", \
                   msg, (size_t)(size), __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_IN_RANGE(value, min, max, msg) \
    do { \
        if ((value) < (min) || (value) > (max)) { \
            printk("    FAIL: %s - value %d not in range [%d, %d] (line %d)\n", \
                   msg, (int)(value), (int)(min), (int)(max), __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

// Convenience aliases for common patterns
#define TEST_ASSERT_NULL(ptr, msg) \
    TEST_ASSERT_PTR_EQ(NULL, ptr, msg)

#define TEST_ASSERT_NOT_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            printk("    FAIL: %s - pointer should not be NULL (line %d)\n", \
                   msg, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_TRUE(condition, msg) \
    TEST_ASSERT_EQ(true, (bool)(condition), msg)

#define TEST_ASSERT_FALSE(condition, msg) \
    TEST_ASSERT_EQ(false, (bool)(condition), msg)

#define TEST_ASSERT_ZERO(value, msg) \
    TEST_ASSERT_EQ(0, value, msg)

#define TEST_ASSERT_POSITIVE(value, msg) \
    TEST_ASSERT((value) > 0, msg " (should be positive)")

#define TEST_ASSERT_NEGATIVE(value, msg) \
    TEST_ASSERT((value) < 0, msg " (should be negative)")

// Legacy aliases
#define TEST_ASSERT_EQUAL(expected, actual, msg) \
    TEST_ASSERT_EQ(expected, actual, msg)

// Result macros
#define TEST_PASS_RESULT() return TEST_PASS
#define TEST_FAIL_RESULT() return TEST_FAIL
#define TEST_SKIP_RESULT() return TEST_SKIP

// Simplified test case definition
#define TEST_CASE(name) {#name, name}

// Simplified test suite definition
#define TEST_SUITE(name, cases, setup_fn, teardown_fn) \
    { \
        name, \
        cases, \
        sizeof(cases) / sizeof(cases[0]), \
        setup_fn, \
        teardown_fn \
    }
