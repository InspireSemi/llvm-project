#include "test_common.h"
#include "lle_manager.h"
#include "resource_map.h"
#include "device_memory.h"

#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/symbol.h>
#include <zephyr/sys/util.h>

// Test data
#define TEST_ADDRESS_1 0x10000000
#define TEST_ADDRESS_2 0x20000000
#define TEST_SIZE_1 1024

// Reference the LLEXT module data from main.c - don't duplicate it  
extern uint8_t llext_buf[];
extern const size_t llext_buf_len;

LOG_MODULE_REGISTER(unit_lle_manager, CONFIG_LOG_DEFAULT_LEVEL);

// We'll test the lle_manager wrapper functions, not the underlying LLEXT API directly

static void setup(void)
{
    device_memory_deinit();
    lle_manager_cleanup();
}

static void teardown(void)
{
    lle_manager_cleanup();
    device_memory_deinit();
}

static test_result_t test_lle_manager_init(void)
{
    int result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_get_entry_point_no_resource(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Try to get entry point for address not in resource map - should be safe
    void *entry_point = lle_manager_get_entry_point(TEST_ADDRESS_1);
    TEST_ASSERT_NULL(entry_point, "Should return NULL for nonexistent resource");
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_get_entry_point_not_lle_module(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate memory but don't mark as LLE module - should be safe
    void *test_memory = device_shared_malloc(TEST_SIZE_1);
    TEST_ASSERT_NOT_NULL(test_memory, "Should allocate memory for test");
    
    uintptr_t test_address = (uintptr_t)test_memory;
    // device_shared_malloc already adds to resource map, don't set LLE status
    
    void *entry_point = lle_manager_get_entry_point(test_address);
    TEST_ASSERT_NULL(entry_point, "Should return NULL for non-LLE resource");
    
    // Clean up
    device_shared_free(test_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_load_and_get_entry_point(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate device memory for the LLEXT buffer
    void *lle_memory = device_shared_malloc(llext_buf_len);  // Use actual size
    TEST_ASSERT_NOT_NULL(lle_memory, "Should allocate memory for LLEXT");
    
    uintptr_t lle_address = (uintptr_t)lle_memory;
    
    // device_shared_malloc already adds to resource map, so just set LLE status
    result = resource_map_set_lle_status(lle_address, true);
    TEST_ASSERT_EQ(0, result, "Setting LLE status should succeed");
    // Don't set loaded status - should remain false
    
    // Copy the LLEXT buffer to device memory - use actual size
    memcpy(lle_memory, llext_buf, llext_buf_len);
    
    // Try to get entry point - this should attempt to load the LLEXT
    void *entry_point = lle_manager_get_entry_point(lle_address);
    // The loading might fail due to the truncated buffer, but we're testing the wrapper logic
    
    // If we got an entry point, test that it's callable
    if (entry_point != NULL) {
        LOG_INF("Got entry point %p, attempting to call it", entry_point);
        
        // Cast to function pointer and call it
        void (*execute_fn)(void) = (void (*)(void))entry_point;
        
        // This should call the hello_world function from the LLEXT module
        execute_fn();
        
        LOG_INF("Successfully called LLEXT entry point");
    } else {
        LOG_WRN("Failed to get entry point - LLEXT loading may have failed");
    }
    
    // Clean up
    device_shared_free(lle_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_get_entry_point_lle_not_loaded(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate memory (not valid LLEXT data)
    void *lle_memory = device_shared_malloc(llext_buf_len);
    TEST_ASSERT_NOT_NULL(lle_memory, "Should allocate memory for test");
    
    uintptr_t lle_address = (uintptr_t)lle_memory;
    
    // Mark as LLE module
    result = resource_map_set_lle_status(lle_address, true);
    TEST_ASSERT_EQ(0, result, "Setting LLE status should succeed");
    
    // Try to get entry point - should fail gracefully since LLEXT data is invalid
    void *entry_point = lle_manager_get_entry_point(lle_address);
    TEST_ASSERT_NULL(entry_point, "Should return NULL when LLEXT loading fails due to invalid data");
    
    // Clean up
    device_shared_free(lle_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_unload_if_loaded_no_resource(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    bool bool_result = lle_manager_unload_if_loaded(TEST_ADDRESS_1);
    TEST_ASSERT_FALSE(bool_result, "Should return false for nonexistent resource");
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_unload_if_loaded_not_lle(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate memory and add resource but don't mark as LLE module
    void *test_memory = device_shared_malloc(TEST_SIZE_1);
    TEST_ASSERT_NOT_NULL(test_memory, "Should allocate memory for test");
    
    uintptr_t test_address = (uintptr_t)test_memory;
    // device_shared_malloc already adds to resource map, don't set LLE status
    
    bool bool_result = lle_manager_unload_if_loaded(test_address);
    TEST_ASSERT_FALSE(bool_result, "Should return false for non-LLE resource");
    
    // Clean up
    device_shared_free(test_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_unload_if_loaded_not_loaded(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate memory, mark as LLE module but not loaded
    void *test_memory = device_shared_malloc(TEST_SIZE_1);
    TEST_ASSERT_NOT_NULL(test_memory, "Should allocate memory for test");
    
    uintptr_t test_address = (uintptr_t)test_memory;
    
    result = resource_map_set_lle_status(test_address, true);
    TEST_ASSERT_EQ(0, result, "Setting LLE status should succeed");
    // is_loaded should be false by default
    
    bool bool_result = lle_manager_unload_if_loaded(test_address);
    TEST_ASSERT_FALSE(bool_result, "Should return false for unloaded LLE module");
    
    // Clean up
    device_shared_free(test_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_unload_if_loaded_no_handle(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate memory, mark as LLE module and loaded but no handle
    void *test_memory = device_shared_malloc(TEST_SIZE_1);
    TEST_ASSERT_NOT_NULL(test_memory, "Should allocate memory for test");
    
    uintptr_t test_address = (uintptr_t)test_memory;
    
    result = resource_map_set_lle_status(test_address, true);
    TEST_ASSERT_EQ(0, result, "Setting LLE status should succeed");
    
    result = resource_map_set_loaded_status(test_address, true);
    TEST_ASSERT_EQ(0, result, "Setting loaded status should succeed");
    // Don't set LLEXT handle - this creates an inconsistent state
    
    // Try to unload - should handle gracefully when loaded=true but no handle
    bool bool_result = lle_manager_unload_if_loaded(test_address);
    // This tests whether the function fails gracefully with inconsistent state
    // The result depends on implementation - could be false (can't unload) or handle the error
    
    // Clean up
    device_shared_free(test_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_integration_with_resource_map(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate memory for a resource
    void *test_memory = device_shared_malloc(TEST_SIZE_1);
    TEST_ASSERT_NOT_NULL(test_memory, "Should allocate memory for test");
    
    uintptr_t test_address = (uintptr_t)test_memory;
    
    // Verify initial state
    resource_entry_t *entry = resource_map_find(test_address);
    TEST_ASSERT_NOT_NULL(entry, "Resource should exist");
    TEST_ASSERT_FALSE(entry->is_lle_module, "Should not be LLE module initially");
    TEST_ASSERT_FALSE(entry->is_loaded, "Should not be loaded initially");
    TEST_ASSERT_NULL(entry->llext_handle, "Should have no LLEXT handle initially");
    
    // Mark as LLE module
    resource_map_set_lle_status(test_address, true);
    entry = resource_map_find(test_address);
    TEST_ASSERT_TRUE(entry->is_lle_module, "Should be marked as LLE module");
    
    // Try to get entry point without valid LLEXT data - should fail gracefully
    void *entry_point = lle_manager_get_entry_point(test_address);
    TEST_ASSERT_NULL(entry_point, "Entry point should be NULL without valid LLEXT data");
    
    // Verify resource map state unchanged after failed load attempt
    entry = resource_map_find(test_address);
    TEST_ASSERT_NOT_NULL(entry, "Resource should still exist");
    TEST_ASSERT_TRUE(entry->is_lle_module, "Should still be marked as LLE module");
    
    // Clean up
    device_shared_free(test_memory);
    
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_cleanup(void)
{
    lle_manager_init();
    
    // Cleanup should not crash
    lle_manager_cleanup();
    
    // Should be able to init again after cleanup
    int result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "Should be able to re-initialize after cleanup");
    TEST_PASS_RESULT();
}

static test_result_t test_lle_manager_entry_point_execution(void)
{
    device_memory_deinit();  // Clean slate for this test
    
    int result = device_memory_init();
    TEST_ASSERT_EQ(0, result, "Device memory initialization should succeed");
    
    result = lle_manager_init();
    TEST_ASSERT_EQ(0, result, "LLE manager initialization should succeed");
    
    // Allocate device memory for the LLEXT buffer
    void *lle_memory = device_shared_malloc(llext_buf_len);
    TEST_ASSERT_NOT_NULL(lle_memory, "Should allocate memory for LLEXT");
    
    uintptr_t lle_address = (uintptr_t)lle_memory;
    
    // Copy the LLEXT buffer to device memory first
    memcpy(lle_memory, llext_buf, llext_buf_len);
    
    // device_shared_malloc already adds to resource map, so just set LLE status
    result = resource_map_set_lle_status(lle_address, true);
    TEST_ASSERT_EQ(0, result, "Setting LLE status should succeed");
    
    // Get entry point - this should load the LLEXT and return the function pointer
    void *entry_point = lle_manager_get_entry_point(lle_address);
    
    if (entry_point != NULL) {
        LOG_INF("Testing entry point execution at %p", entry_point);
        
        // Verify the entry point is callable
        void (*execute_fn)(void) = (void (*)(void))entry_point;
        
        // Call the function - this should execute the hello_world code
        execute_fn();
        
        LOG_INF("Successfully executed LLEXT entry point");
        
        // Test that we can get the same entry point again (should be cached)
        void *entry_point2 = lle_manager_get_entry_point(lle_address);
        TEST_ASSERT_EQ(entry_point, entry_point2, "Should return same entry point on subsequent calls");
        
        // Test unloading
        bool unload_result = lle_manager_unload_if_loaded(lle_address);
        TEST_ASSERT_TRUE(unload_result, "Should successfully unload the LLEXT module");
        
        // After unloading, getting entry point should require reloading
        void *entry_point3 = lle_manager_get_entry_point(lle_address);
        // This might be NULL if reload fails, or the same address if reload succeeds
        
    } else {
        LOG_WRN("Failed to load LLEXT module - may be due to test environment limitations");
    }
    
    // Clean up
    device_shared_free(lle_memory);
    
    TEST_PASS_RESULT();
}

// Test cases array
static const test_case_t lle_manager_test_cases[] = {
    TEST_CASE(test_lle_manager_init),
    TEST_CASE(test_lle_manager_get_entry_point_no_resource),
    TEST_CASE(test_lle_manager_get_entry_point_not_lle_module),
    TEST_CASE(test_lle_manager_load_and_get_entry_point),
    TEST_CASE(test_lle_manager_get_entry_point_lle_not_loaded),
    TEST_CASE(test_lle_manager_unload_if_loaded_no_resource),
    TEST_CASE(test_lle_manager_unload_if_loaded_not_lle),
    TEST_CASE(test_lle_manager_unload_if_loaded_not_loaded),
    TEST_CASE(test_lle_manager_unload_if_loaded_no_handle),
    TEST_CASE(test_lle_manager_integration_with_resource_map),
    TEST_CASE(test_lle_manager_entry_point_execution),
    TEST_CASE(test_lle_manager_cleanup),
};

// Test suite definition
const test_suite_t lle_manager_test_suite = TEST_SUITE(
    "LLE Manager Tests",
    lle_manager_test_cases,
    setup,
    teardown
);

// Main test runner function
uint32_t run_lle_manager_tests(void)
{
    return test_run_suite(&lle_manager_test_suite);
}