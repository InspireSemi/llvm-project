/**
 * @file unit_ivshmem_heap.c
 * @brief Unit tests for IVSHMEM heap functionality
 */

#include "unit_test_framework.h"
#include "ivshmem_heap.h"
#include "device_memory.h"
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(unit_ivshmem_heap, CONFIG_LOG_DEFAULT_LEVEL);

static void ivshmem_heap_test_setup(void)
{
    // Ensure device memory is initialized before each test
    // This is safe to call multiple times - it will return 0 if already initialized
    int result = device_memory_init();
    if (result != 0) {
        printk("ERROR: Failed to initialize device memory in IVSHMEM heap test setup\n");
    }
}

static test_result_t test_ivshmem_heap_get(void)
{
    struct k_heap *heap = ivshmem_heap_get();
    TEST_ASSERT_NOT_NULL(heap, "Heap handle should not be NULL");
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_heap_bytes(void)
{
    size_t heap_size = ivshmem_heap_bytes();
    TEST_ASSERT_TRUE(heap_size > 0, "Heap size should be positive");
    TEST_ASSERT_TRUE(heap_size < SIZE_MAX, "Heap size should be reasonable");
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_malloc_basic(void)
{
    void *ptr = ivshmem_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Basic allocation should succeed");
    
    // Write to allocated memory to verify it's accessible
    memset(ptr, 0xAA, 64);
    
    ivshmem_free(ptr);
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_malloc_zero(void)
{
    void *ptr = ivshmem_malloc(0);
    TEST_ASSERT_NULL(ptr, "Zero-size allocation should return NULL");
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_malloc_large(void)
{
    size_t heap_size = ivshmem_heap_bytes();
    void *ptr = ivshmem_malloc(heap_size + 1024);
    TEST_ASSERT_NULL(ptr, "Over-sized allocation should fail");
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_free_null(void)
{
    // This should not crash
    ivshmem_free(NULL);
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_free_valid(void)
{
    void *ptr = ivshmem_malloc(128);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    // Free should not crash
    ivshmem_free(ptr);
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_malloc_free_cycle(void)
{
    const int iterations = 10;
    
    for (int i = 0; i < iterations; i++) {
        void *ptr = ivshmem_malloc(64 + i * 16);
        TEST_ASSERT_NOT_NULL(ptr, "Allocation in cycle should succeed");
        
        // Write pattern to verify memory
        memset(ptr, i & 0xFF, 64 + i * 16);
        
        ivshmem_free(ptr);
    }
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_malloc_alignment(void)
{
    void *ptr = ivshmem_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    // Check alignment (typically 4 or 8 byte aligned)
    uintptr_t addr = (uintptr_t)ptr;
    TEST_ASSERT_TRUE((addr % sizeof(void*)) == 0, "Pointer should be aligned");
    
    ivshmem_free(ptr);
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_multiple_allocations(void)
{
    const int num_allocs = 5;
    void *ptrs[num_allocs];
    
    // Allocate multiple blocks
    for (int i = 0; i < num_allocs; i++) {
        ptrs[i] = ivshmem_malloc(32 * (i + 1));
        TEST_ASSERT_NOT_NULL(ptrs[i], "Multiple allocations should succeed");
        
        // Verify pointers are unique
        for (int j = 0; j < i; j++) {
            TEST_ASSERT_TRUE(ptrs[i] != ptrs[j], "Pointers should be unique");
        }
    }
    
    // Free all blocks
    for (int i = 0; i < num_allocs; i++) {
        ivshmem_free(ptrs[i]);
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_heap_exhaustion(void)
{
    size_t heap_size = ivshmem_heap_bytes();
    void **ptrs = k_malloc(sizeof(void*) * 100);
    TEST_ASSERT_NOT_NULL(ptrs, "Test setup allocation should succeed");
    
    int alloc_count = 0;
    size_t total_allocated = 0;
    
    // Try to exhaust the heap
    while (total_allocated < heap_size && alloc_count < 100) {
        size_t alloc_size = 1024;
        ptrs[alloc_count] = ivshmem_malloc(alloc_size);
        
        if (ptrs[alloc_count] == NULL) {
            break; // Heap exhausted
        }
        
        total_allocated += alloc_size;
        alloc_count++;
    }
    
    // Verify we allocated something
    TEST_ASSERT_TRUE(alloc_count > 0, "Should have made some allocations");
    
    // Clean up
    for (int i = 0; i < alloc_count; i++) {
        ivshmem_free(ptrs[i]);
    }
    k_free(ptrs);
    
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_double_free(void)
{
    void *ptr = ivshmem_malloc(64);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    ivshmem_free(ptr);
    
    // Double free should not crash (though behavior may be undefined)
    ivshmem_free(ptr);
    
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_fragmentation(void)
{
    const int num_blocks = 10;
    void *ptrs[num_blocks];
    
    // Allocate blocks
    for (int i = 0; i < num_blocks; i++) {
        ptrs[i] = ivshmem_malloc(128);
        TEST_ASSERT_NOT_NULL(ptrs[i], "Fragmentation test allocation should succeed");
    }
    
    // Free every other block to create fragmentation
    for (int i = 1; i < num_blocks; i += 2) {
        ivshmem_free(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    // Try to allocate a larger block that might fit in fragmented space
    void *large_ptr = ivshmem_malloc(256);
    // Note: This might fail due to fragmentation, which is expected behavior
    if (large_ptr != NULL) {
        // If allocation succeeded, free it
        ivshmem_free(large_ptr);
    } else {
        LOG_INF("Fragmentation test: Large allocation failed as expected");
    }
    
    // Clean up remaining allocations
    for (int i = 0; i < num_blocks; i++) {
        if (ptrs[i] != NULL) {
            ivshmem_free(ptrs[i]);
        }
    }
    
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_heap_consistency(void)
{
    // Get initial heap state
    size_t initial_heap_size = ivshmem_heap_bytes();
    
    // Perform some operations
    void *ptr1 = ivshmem_malloc(256);
    void *ptr2 = ivshmem_malloc(512);
    
    TEST_ASSERT_NOT_NULL(ptr1, "First allocation should succeed");
    TEST_ASSERT_NOT_NULL(ptr2, "Second allocation should succeed");
    
    ivshmem_free(ptr1);
    ivshmem_free(ptr2);
    
    // Verify heap size is still the same
    size_t final_heap_size = ivshmem_heap_bytes();
    TEST_ASSERT_EQUAL(initial_heap_size, final_heap_size, "Heap size should remain consistent");
    
    TEST_PASS_RESULT();
}

static const test_case_t ivshmem_heap_test_cases[] = {
    TEST_CASE(test_ivshmem_heap_get),
    TEST_CASE(test_ivshmem_heap_bytes),
    TEST_CASE(test_ivshmem_malloc_basic),
    TEST_CASE(test_ivshmem_malloc_zero),
    TEST_CASE(test_ivshmem_malloc_large),
    TEST_CASE(test_ivshmem_free_null),
    TEST_CASE(test_ivshmem_free_valid),
    TEST_CASE(test_ivshmem_malloc_free_cycle),
    TEST_CASE(test_ivshmem_malloc_alignment),
    TEST_CASE(test_ivshmem_multiple_allocations),
    TEST_CASE(test_ivshmem_heap_exhaustion),
    TEST_CASE(test_ivshmem_double_free),
    TEST_CASE(test_ivshmem_fragmentation),
    TEST_CASE(test_ivshmem_heap_consistency)
};

static const test_suite_t ivshmem_heap_test_suite = TEST_SUITE(
    "IVSHMEM Heap Tests",
    ivshmem_heap_test_cases,
    ivshmem_heap_test_setup,
    NULL
);

uint32_t run_ivshmem_heap_unit_tests(void)
{
    return test_run_suite(&ivshmem_heap_test_suite);
}
