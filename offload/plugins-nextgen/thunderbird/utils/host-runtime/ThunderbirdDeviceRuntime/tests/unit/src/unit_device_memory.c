/**
 * @file unit_heap.c
 * @brief Unit tests for IVSHMEM heap functionality
 */

#include "unit_test_framework.h"
#include "device_memory.h"
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(unit_device_memory, CONFIG_LOG_DEFAULT_LEVEL);

static void device_memory_test_setup(void)
{
    // Ensure device memory is initialized before each test
    // This is safe to call multiple times - it will return 0 if already initialized
    int result = device_memory_init();
    if (result != 0) {
        printk("ERROR: Failed to initialize device memory in test setup\n");
    }
}

static test_result_t test_heap_init(void)
{
    // Heap is automatically initialized via SYS_INIT - just verify it's working
    void* ptr = device_shared_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Memory allocation should work after auto-initialization");
    device_shared_free(ptr);
    TEST_PASS_RESULT();
}

static test_result_t test_heap_get_handle(void)
{
    // Note: device_memory API doesn't expose internal heap handle
    // This test just verifies initialization doesn't break subsequent operations
    void* ptr = device_shared_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Memory allocation after init should work");
    device_shared_free(ptr);
    TEST_PASS_RESULT();
}

static test_result_t test_malloc_basic(void)
{
    void* ptr = device_shared_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Basic allocation should succeed");
    device_shared_free(ptr);
    TEST_PASS_RESULT();
}

static test_result_t test_malloc_zero(void)
{
    void* ptr = device_shared_malloc(0);
    TEST_ASSERT_NULL(ptr, "alloc(0) should return NULL");
    TEST_PASS_RESULT();
}

static test_result_t test_heap_null_pointers(void)
{
    device_shared_free(NULL);  // Should not crash
    device_exclusive_free(NULL);  // Should not crash
    
    void* ptr1 = device_shared_malloc(0);
    TEST_ASSERT_NULL(ptr1, "shared alloc(0) should return NULL");
    
    void* ptr2 = device_exclusive_malloc(0);
    TEST_ASSERT_NULL(ptr2, "exclusive alloc(0) should return NULL");
    
    TEST_PASS_RESULT();
}

static test_result_t test_free_null(void)
{
    device_shared_free(NULL);  // Should not crash
    TEST_PASS_RESULT();
}

static test_result_t test_malloc_free_cycle(void)
{
    void* ptr1 = device_shared_malloc(128);
    TEST_ASSERT_NOT_NULL(ptr1, "First allocation should succeed");
    
    void* ptr2 = device_shared_malloc(256);
    TEST_ASSERT_NOT_NULL(ptr2, "Second allocation should succeed");
    
    device_shared_free(ptr1);
    device_shared_free(ptr2);
    
    void* ptr3 = device_shared_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr3, "Third allocation should succeed");
    device_shared_free(ptr3);
    
    TEST_PASS_RESULT();
}

static test_result_t test_heap_oversized_allocation(void)
{
    size_t huge_size = SIZE_MAX;
    void* ptr = device_shared_malloc(huge_size);
    TEST_ASSERT_NULL(ptr, "Oversized allocation should fail");
    
    TEST_PASS_RESULT();
}

static test_result_t test_heap_double_free(void)
{
    void* ptr = device_shared_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    device_shared_free(ptr);
    device_shared_free(ptr);  // Double free - should be safe
    
    TEST_PASS_RESULT();
}

static test_result_t test_cross_free_shared_to_exclusive(void)
{
    void* shared_ptr = device_shared_malloc(256);
    TEST_ASSERT_NOT_NULL(shared_ptr, "Shared allocation should succeed");
    
    // Attempt to free shared pointer with exclusive free - should be safe but logged
    LOG_DBG("Freeing shared pointer with exclusive free");
    device_exclusive_free(shared_ptr);
    
    LOG_DBG("Freeing shared pointer with shared free");
    // The pointer should still be valid in shared memory, so we free it properly
    device_shared_free(shared_ptr);
    
    TEST_PASS_RESULT();
}

static test_result_t test_cross_free_exclusive_to_shared(void)
{
    void* exclusive_ptr = device_exclusive_malloc(256);
    TEST_ASSERT_NOT_NULL(exclusive_ptr, "Exclusive allocation should succeed");
    
    // Attempt to free exclusive pointer with shared free - should be safe but logged
    device_shared_free(exclusive_ptr);
    
    // The pointer should still be valid in exclusive memory, so we free it properly
    device_exclusive_free(exclusive_ptr);
    
    TEST_PASS_RESULT();
}

static const test_case_t device_memory_test_cases[] = {
    TEST_CASE(test_heap_init),
    TEST_CASE(test_heap_get_handle),
    TEST_CASE(test_malloc_basic),
    TEST_CASE(test_malloc_zero),
    TEST_CASE(test_heap_null_pointers),
    TEST_CASE(test_free_null),
    TEST_CASE(test_malloc_free_cycle),
    TEST_CASE(test_heap_oversized_allocation),
    TEST_CASE(test_heap_double_free),
    TEST_CASE(test_cross_free_shared_to_exclusive),
    TEST_CASE(test_cross_free_exclusive_to_shared)
};

static const test_suite_t device_memory_test_suite = TEST_SUITE(
    "Device Memory and Error Handling Tests",
    device_memory_test_cases,
    device_memory_test_setup,
    NULL
);

uint32_t run_device_memory_unit_tests(void)
{
    return test_run_suite(&device_memory_test_suite);
}
