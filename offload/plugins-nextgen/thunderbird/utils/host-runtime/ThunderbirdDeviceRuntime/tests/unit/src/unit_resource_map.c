#include "test_common.h"
#include "resource_map.h"

// Test data
#define TEST_ADDRESS_1 0x10000000
#define TEST_ADDRESS_2 0x20000000
#define TEST_ADDRESS_3 0x30000000
#define TEST_SIZE_1 1024
#define TEST_SIZE_2 2048
#define TEST_SIZE_3 4096

static void setup(void)
{
    resource_map_cleanup();
}

static void teardown(void)
{
    resource_map_cleanup();
}

static test_result_t test_resource_map_init(void)
{
    int result = resource_map_init();
    TEST_ASSERT_EQ(0, result, "Resource map initialization should succeed");
    TEST_ASSERT_EQ(0, resource_map_get_count(), "Initial count should be 0");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_add_basic(void)
{
    resource_map_init();
    
    int result = resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true);
    TEST_ASSERT_EQ(0, result, "Adding valid resource should succeed");
    TEST_ASSERT_EQ(1, resource_map_get_count(), "Count should be 1 after adding");
    
    resource_entry_t *entry = resource_map_find(TEST_ADDRESS_1);
    TEST_ASSERT_NOT_NULL(entry, "Should find added entry");
    TEST_ASSERT_EQ(TEST_ADDRESS_1, entry->address, "Address should match");
    TEST_ASSERT_EQ(TEST_SIZE_1, entry->size, "Size should match");
    TEST_ASSERT_TRUE(entry->is_shared_memory, "Should be marked as shared memory");
    TEST_ASSERT_FALSE(entry->is_lle_module, "Should not be LLE module initially");
    TEST_ASSERT_FALSE(entry->is_loaded, "Should not be loaded initially");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_add_multiple(void)
{
    resource_map_init();
    
    TEST_ASSERT_EQ(0, resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true), "First add should succeed");
    TEST_ASSERT_EQ(0, resource_map_add(TEST_ADDRESS_2, TEST_SIZE_2, false), "Second add should succeed");
    TEST_ASSERT_EQ(0, resource_map_add(TEST_ADDRESS_3, TEST_SIZE_3, true), "Third add should succeed");
    
    TEST_ASSERT_EQ(3, resource_map_get_count(), "Count should be 3");
    
    // Verify all entries exist
    TEST_ASSERT_NOT_NULL(resource_map_find(TEST_ADDRESS_1), "Should find first entry");
    TEST_ASSERT_NOT_NULL(resource_map_find(TEST_ADDRESS_2), "Should find second entry");
    TEST_ASSERT_NOT_NULL(resource_map_find(TEST_ADDRESS_3), "Should find third entry");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_add_duplicate(void)
{
    resource_map_init();
    
    TEST_ASSERT_EQ(0, resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true), "First add should succeed");
    
    // Adding duplicate should fail
    int result = resource_map_add(TEST_ADDRESS_1, TEST_SIZE_2, false);
    TEST_ASSERT_NEQ(0, result, "Adding duplicate address should fail");
    TEST_ASSERT_EQ(1, resource_map_get_count(), "Count should remain 1");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_add_invalid_size(void)
{
    resource_map_init();
    
    int result = resource_map_add(TEST_ADDRESS_1, 0, true);
    TEST_ASSERT_NEQ(0, result, "Adding zero size should fail");
    TEST_ASSERT_EQ(0, resource_map_get_count(), "Count should remain 0");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_remove_basic(void)
{
    resource_map_init();
    resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true);
    resource_map_add(TEST_ADDRESS_2, TEST_SIZE_2, false);
    
    int result = resource_map_remove(TEST_ADDRESS_1);
    TEST_ASSERT_EQ(0, result, "Remove should succeed");
    TEST_ASSERT_EQ(1, resource_map_get_count(), "Count should be 1 after removal");
    
    TEST_ASSERT_NULL(resource_map_find(TEST_ADDRESS_1), "Removed entry should not be found");
    TEST_ASSERT_NOT_NULL(resource_map_find(TEST_ADDRESS_2), "Other entry should still exist");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_remove_nonexistent(void)
{
    resource_map_init();
    
    int result = resource_map_remove(TEST_ADDRESS_1);
    TEST_ASSERT_NEQ(0, result, "Removing nonexistent entry should fail");
    TEST_ASSERT_EQ(0, resource_map_get_count(), "Count should remain 0");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_find_nonexistent(void)
{
    resource_map_init();
    
    resource_entry_t *entry = resource_map_find(TEST_ADDRESS_1);
    TEST_ASSERT_NULL(entry, "Should not find nonexistent entry");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_set_lle_status(void)
{
    resource_map_init();
    resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true);
    
    int result = resource_map_set_lle_status(TEST_ADDRESS_1, true);
    TEST_ASSERT_EQ(0, result, "Setting LLE status should succeed");
    
    resource_entry_t *entry = resource_map_find(TEST_ADDRESS_1);
    TEST_ASSERT_NOT_NULL(entry, "Entry should exist");
    TEST_ASSERT_TRUE(entry->is_lle_module, "Should be marked as LLE module");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_set_loaded_status(void)
{
    resource_map_init();
    resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true);
    
    int result = resource_map_set_loaded_status(TEST_ADDRESS_1, true);
    TEST_ASSERT_EQ(0, result, "Setting loaded status should succeed");
    
    resource_entry_t *entry = resource_map_find(TEST_ADDRESS_1);
    TEST_ASSERT_NOT_NULL(entry, "Entry should exist");
    TEST_ASSERT_TRUE(entry->is_loaded, "Should be marked as loaded");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_set_status_nonexistent(void)
{
    resource_map_init();
    
    int result1 = resource_map_set_lle_status(TEST_ADDRESS_1, true);
    TEST_ASSERT_NEQ(0, result1, "Setting LLE status on nonexistent entry should fail");
    
    int result2 = resource_map_set_loaded_status(TEST_ADDRESS_1, true);
    TEST_ASSERT_NEQ(0, result2, "Setting loaded status on nonexistent entry should fail");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_llext_handle(void)
{
    resource_map_init();
    resource_map_add(TEST_ADDRESS_1, TEST_SIZE_1, true);
    
    // Use a dummy pointer for testing
    struct llext *dummy_handle = (struct llext *)0xDEADBEEF;
    
    int result = resource_map_set_llext_handle(TEST_ADDRESS_1, dummy_handle);
    TEST_ASSERT_EQ(0, result, "Setting LLEXT handle should succeed");
    
    struct llext *retrieved = resource_map_get_llext_handle(TEST_ADDRESS_1);
    TEST_ASSERT_PTR_EQ(dummy_handle, retrieved, "Retrieved handle should match");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_llext_handle_nonexistent(void)
{
    resource_map_init();
    
    struct llext *dummy_handle = (struct llext *)0xDEADBEEF;
    int result = resource_map_set_llext_handle(TEST_ADDRESS_1, dummy_handle);
    TEST_ASSERT_NEQ(0, result, "Setting handle on nonexistent entry should fail");
    
    struct llext *retrieved = resource_map_get_llext_handle(TEST_ADDRESS_1);
    TEST_ASSERT_NULL(retrieved, "Should not retrieve handle for nonexistent entry");
    TEST_PASS_RESULT();
}

static test_result_t test_resource_map_capacity_growth(void)
{
    resource_map_init();
    
    // Add many entries to trigger capacity growth
    for (int i = 0; i < 20; i++) {
        uintptr_t addr = TEST_ADDRESS_1 + (i * 0x1000);
        int result = resource_map_add(addr, TEST_SIZE_1, true);
        TEST_ASSERT_EQ(0, result, "Adding entry should succeed");
    }
    
    TEST_ASSERT_EQ(20, resource_map_get_count(), "Count should be 20");
    
    // Verify all entries are still accessible
    for (int i = 0; i < 20; i++) {
        uintptr_t addr = TEST_ADDRESS_1 + (i * 0x1000);
        resource_entry_t *entry = resource_map_find(addr);
        TEST_ASSERT_NOT_NULL(entry, "Entry should still exist after growth");
    }
    TEST_PASS_RESULT();
}

// Test cases array
static const test_case_t resource_map_test_cases[] = {
    TEST_CASE(test_resource_map_init),
    TEST_CASE(test_resource_map_add_basic),
    TEST_CASE(test_resource_map_add_multiple),
    TEST_CASE(test_resource_map_add_duplicate),
    TEST_CASE(test_resource_map_add_invalid_size),
    TEST_CASE(test_resource_map_remove_basic),
    TEST_CASE(test_resource_map_remove_nonexistent),
    TEST_CASE(test_resource_map_find_nonexistent),
    TEST_CASE(test_resource_map_set_lle_status),
    TEST_CASE(test_resource_map_set_loaded_status),
    TEST_CASE(test_resource_map_set_status_nonexistent),
    TEST_CASE(test_resource_map_llext_handle),
    TEST_CASE(test_resource_map_llext_handle_nonexistent),
    TEST_CASE(test_resource_map_capacity_growth),
};

// Test suite definition
const test_suite_t resource_map_test_suite = TEST_SUITE(
    "Resource Map Tests",
    resource_map_test_cases,
    setup,
    teardown
);

// Main test runner function
uint32_t run_resource_map_tests(void)
{
    return test_run_suite(&resource_map_test_suite);
}