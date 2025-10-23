/**
 * @file unit_test_framework.c
 * @brief Implementation of the unit test runner using common framework
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-10-07
 */

#include "unit_test_framework.h"
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(unit_test_framework, LOG_LEVEL_DBG);

// Forward declarations of test runner functions
extern uint32_t run_device_memory_unit_tests(void);
extern uint32_t run_mailbox_unit_tests(void);
extern uint32_t run_mailbox_operations_unit_tests(void);
extern uint32_t run_mailbox_region_unit_tests(void);
extern uint32_t run_message_slot_unit_tests(void);
extern uint32_t run_message_payloads_unit_tests(void);
extern uint32_t run_ivshmem_shm_unit_tests(void);
extern uint32_t run_ivshmem_heap_unit_tests(void);
extern uint32_t run_resource_map_tests(void);
extern uint32_t run_lle_manager_tests(void);
extern uint32_t run_kernel_processor_tests(void);
extern uint32_t run_cache_map_tests(void);  // Added for cache map tests

// Main unit test runner function
int run_all_unit_tests(void) {
    printk("\n=================================================================\n");
    printk("  Thunderbird Device Runtime - Comprehensive Unit Test Suite\n");
    printk("=================================================================\n");

    // Initialize the common test framework
    test_framework_init();

    // Run new resource management and kernel processing tests
    // run_resource_map_tests();
    // run_lle_manager_tests();
    // run_kernel_processor_tests(); // Dependent on staticly loaded lext, needs to be reworked.
    run_cache_map_tests();

    // Keep existing working tests
    run_device_memory_unit_tests();
    // run_ivshmem_heap_unit_tests();
    // run_ivshmem_shm_unit_tests();
    // run_mailbox_region_unit_tests(); // DISABLED: All tests are empty TODOs that just call TEST_PASS_RESULT()
    // run_mailbox_unit_tests();
    
    // Enable new stub tests
    // run_new_mailbox_operations_tests();
    // run_new_message_payloads_tests(); // DISABLED: All tests were placeholder tests with no validation
    // run_new_message_slot_tests();
    // run_batch_controller_tests();
    // run_batch_state_tests();
    // run_message_router_tests();
  
    // Print overall summary
    test_print_summary();
    
    test_stats_t stats = test_get_stats();
    printk("\n=================================================================\n");
    if (stats.failed_tests == 0) {
        printk("🎉 ALL UNIT TESTS PASSED! 🎉\n");
    } else {
        printk("❌ %u tests failed. Review output for details.\n", stats.failed_tests);
    }
    printk("=================================================================\n");
    
    // Return standard program exit codes: 0 = success, 1 = failure
    return (stats.failed_tests == 0) ? 0 : 1;
}
