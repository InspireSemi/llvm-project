/**
 * @file test_common.c
 * @brief Unified test framework implementation
 */

#include "test_common.h"

static test_stats_t g_test_stats = {0};
static const char* g_current_test_name = NULL;

void test_framework_init(void)
{
    memset(&g_test_stats, 0, sizeof(g_test_stats));
}

test_stats_t test_get_stats(void)
{
    return g_test_stats;
}

void test_reset_stats(void)
{
    memset(&g_test_stats, 0, sizeof(g_test_stats));
}

uint32_t test_run_suite(const test_suite_t* suite)
{
    if (!suite) {
        return 0;
    }

    printk("\n=== %s ===\n", suite->name);
    
    uint32_t suite_passed = 0;
    
    // Run setup if provided
    if (suite->setup) {
        suite->setup();
    }
    
    // Run each test case
    for (uint32_t i = 0; i < suite->num_cases; i++) {
        const test_case_t* test_case = &suite->test_cases[i];
        g_current_test_name = test_case->name;
        
        printk("  %s... ", test_case->name);
        
        test_result_t result = test_case->test_func();
        g_test_stats.total_tests++;
        
        switch (result) {
            case TEST_PASS:
                printk("PASS\n");
                g_test_stats.passed_tests++;
                suite_passed++;
                break;
            case TEST_FAIL:
                printk("FAIL\n");
                g_test_stats.failed_tests++;
                break;
            case TEST_SKIP:
                printk("SKIP\n");
                g_test_stats.skipped_tests++;
                break;
        }
    }
    
    // Run teardown if provided
    if (suite->teardown) {
        suite->teardown();
    }
    
    g_current_test_name = NULL;
    return suite_passed;
}

uint32_t test_run_case(const test_case_t* test_case)
{
    if (!test_case) {
        return 0;
    }
    
    g_current_test_name = test_case->name;
    printk("  %s... ", test_case->name);
    
    test_result_t result = test_case->test_func();
    g_test_stats.total_tests++;
    
    uint32_t passed = 0;
    switch (result) {
        case TEST_PASS:
            printk("PASS\n");
            g_test_stats.passed_tests++;
            passed = 1;
            break;
        case TEST_FAIL:
            printk("FAIL\n");
            g_test_stats.failed_tests++;
            break;
        case TEST_SKIP:
            printk("SKIP\n");
            g_test_stats.skipped_tests++;
            break;
    }
    
    g_current_test_name = NULL;
    return passed;
}

void test_print_summary(void)
{
    printk("\n=== Test Summary ===\n");
    printk("Total: %u, Passed: %u, Failed: %u, Skipped: %u\n",
           g_test_stats.total_tests,
           g_test_stats.passed_tests, 
           g_test_stats.failed_tests,
           g_test_stats.skipped_tests);
    
    if (g_test_stats.total_tests > 0) {
        uint32_t success_rate = (g_test_stats.passed_tests * 100) / g_test_stats.total_tests;
        printk("Success Rate: %u%%\n", success_rate);
    }
    
    if (g_test_stats.failed_tests == 0) {
        printk("🎉 ALL TESTS PASSED!\n");
    }
}

const char* test_get_current_name(void)
{
    return g_current_test_name;
}
