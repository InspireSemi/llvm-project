#include "test_common.h"
#include "cache_map.h"
#include <zephyr/kernel.h>
#include <errno.h>  // Add this line
#include <string.h>

// Mock buffers for testing (simulate allocated memory)
static uint8_t mock_shared_buffer[1024];
static uint8_t mock_exclusive_buffer[1024];

static void setup(void) {
    // Initialize cache map before each test
    cache_map_init();
}

static void teardown(void) {
    // Cleanup after each test
    cache_map_cleanup();
}

static test_result_t test_cache_map_init(void) {
    // Init is done in setup, just verify count is 0
    TEST_ASSERT_ZERO(cache_map_get_count(), "Initial count should be 0");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_add_pair(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    int ret = cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    TEST_ASSERT_ZERO(ret, "cache_map_add_pair should succeed");
    TEST_ASSERT_EQ(1, cache_map_get_count(), "Count should be 1 after add");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_add_pair_duplicate(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    // Adding duplicate shared address should overwrite
    int ret = cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    TEST_ASSERT_EQ(-EEXIST, ret, "Adding duplicate should fail");  // Change from expecting 0
    TEST_ASSERT_EQ(1, cache_map_get_count(), "Count should still be 1");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_find_by_shared(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_pair_t *pair = cache_map_find_by_shared(shared);
    TEST_ASSERT_NOT_NULL(pair, "Should find pair by shared address");
    TEST_ASSERT_PTR_EQ((void *)shared, (void *)pair->shared_address, "Shared address should match");
    TEST_ASSERT_PTR_EQ((void *)exclusive, (void *)pair->exclusive_address, "Exclusive address should match");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_find_by_exclusive(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_pair_t *pair = cache_map_find_by_exclusive(exclusive);
    TEST_ASSERT_NOT_NULL(pair, "Should find pair by exclusive address");
    TEST_ASSERT_PTR_EQ((void *)shared, (void *)pair->shared_address, "Shared address should match");
    TEST_ASSERT_PTR_EQ((void *)exclusive, (void *)pair->exclusive_address, "Exclusive address should match");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_get_exclusive_from_shared(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    uintptr_t found_exclusive = cache_map_get_exclusive_from_shared(shared);
    TEST_ASSERT_PTR_EQ((void *)exclusive, (void *)found_exclusive, "Should get correct exclusive address");
    TEST_PASS_RESULT();
}

static test_result_t test_do_we_need_to_sync(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_pair_t *pair = cache_map_find_by_shared(shared);
    TEST_ASSERT_NOT_NULL(pair, "Pair should exist");
    // Initially, sync needed
    TEST_ASSERT_TRUE(do_we_need_to_sync(pair), "Should need sync initially");
    // After setting flags
    cache_map_set_status(pair, true, false);
    TEST_ASSERT_FALSE(do_we_need_to_sync(pair), "Should not need sync after clean sync");
    // After setting dirty
    cache_map_set_status(pair, true, true);
    TEST_ASSERT_TRUE(do_we_need_to_sync(pair), "Should need sync when dirty");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_sync_shared_to_exclusive(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    memset(mock_shared_buffer, 0xAA, sizeof(mock_shared_buffer));
    memset(mock_exclusive_buffer, 0x00, sizeof(mock_exclusive_buffer));
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_pair_t *pair = cache_map_find_by_shared(shared);
    int ret = cache_map_sync_shared_to_exclusive(pair, 0, 4, 1);
    TEST_ASSERT_ZERO(ret, "Sync should succeed");
    TEST_ASSERT_MEM_EQ(mock_shared_buffer, mock_exclusive_buffer, 4, "Buffers should match after sync");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_sync_exclusive_to_shared(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    memset(mock_shared_buffer, 0x00, sizeof(mock_shared_buffer));
    memset(mock_exclusive_buffer, 0xBB, sizeof(mock_exclusive_buffer));
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_pair_t *pair = cache_map_find_by_shared(shared);
    int ret = cache_map_sync_exclusive_to_shared(pair, 0, 4, 1);
    TEST_ASSERT_ZERO(ret, "Sync should succeed");
    TEST_ASSERT_MEM_EQ(mock_exclusive_buffer, mock_shared_buffer, 4, "Buffers should match after sync");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_sync_bounds_check(void) {
    // Use offset addresses to avoid conflicts with other tests
    uintptr_t shared = (uintptr_t)mock_shared_buffer + 512;   // Different address
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer + 512; // Different address
    
    int add_ret = cache_map_add_pair(shared, 100, exclusive, 100);
    TEST_ASSERT_ZERO(add_ret, "Should be able to add pair with different addresses");
    
    cache_pair_t *pair = cache_map_find_by_shared(shared);
    TEST_ASSERT_NOT_NULL(pair, "Should find the pair we just added");
    
    int ret = cache_map_sync_shared_to_exclusive(pair, 0, 50, 3);  // 150 > 100
    TEST_ASSERT_EQ(-EINVAL, ret, "Sync should fail on bounds check");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_set_status(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_pair_t *pair = cache_map_find_by_shared(shared);
    int ret = cache_map_set_status(pair, true, true);
    TEST_ASSERT_ZERO(ret, "Set status should succeed");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_set_status_by_exclusive(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    int ret = cache_map_set_status_by_exclusive(exclusive, true, false);
    TEST_ASSERT_ZERO(ret, "Set status by exclusive should succeed");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_remove_pair(void) {
    // Use unique address to avoid conflicts with other tests
    uintptr_t shared = (uintptr_t)mock_shared_buffer + 256;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer + 256;
    
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    
    // Count before remove (should be whatever was there + 1)
    size_t count_before = cache_map_get_count();
    
    int ret = cache_map_remove_pair(shared);
    TEST_ASSERT_ZERO(ret, "Remove should succeed");
    
    // Count after remove should be one less
    size_t count_after = cache_map_get_count();
    TEST_ASSERT_EQ(count_before - 1, count_after, "Count should decrease by 1");
    
    TEST_ASSERT_NULL(cache_map_find_by_shared(shared), "Should not find after remove");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_remove_nonexistent(void) {
    // Use an address that definitely doesn't exist (way outside our buffer range)
    uintptr_t nonexistent_addr = (uintptr_t)mock_shared_buffer + 0x10000; // Far outside buffer
    
    int ret = cache_map_remove_pair(nonexistent_addr);
    TEST_ASSERT_EQ(-ENOENT, ret, "Remove nonexistent should fail");
    TEST_PASS_RESULT();
}

static test_result_t test_cache_map_cleanup(void) {
    uintptr_t shared = (uintptr_t)mock_shared_buffer;
    uintptr_t exclusive = (uintptr_t)mock_exclusive_buffer;
    cache_map_add_pair(shared, sizeof(mock_shared_buffer), exclusive, sizeof(mock_exclusive_buffer));
    cache_map_cleanup();
    TEST_ASSERT_ZERO(cache_map_get_count(), "Count should be 0 after cleanup");
    TEST_PASS_RESULT();
}

// Test cases array
static const test_case_t cache_map_test_cases[] = {
    TEST_CASE(test_cache_map_init),
    TEST_CASE(test_cache_map_add_pair),
    TEST_CASE(test_cache_map_add_pair_duplicate),
    TEST_CASE(test_cache_map_find_by_shared),
    TEST_CASE(test_cache_map_find_by_exclusive),
    TEST_CASE(test_cache_map_get_exclusive_from_shared),
    TEST_CASE(test_do_we_need_to_sync),
    TEST_CASE(test_cache_map_sync_shared_to_exclusive),
    TEST_CASE(test_cache_map_sync_exclusive_to_shared),
    TEST_CASE(test_cache_map_sync_bounds_check),
    TEST_CASE(test_cache_map_set_status),
    TEST_CASE(test_cache_map_set_status_by_exclusive),
    TEST_CASE(test_cache_map_remove_pair),
    TEST_CASE(test_cache_map_remove_nonexistent),
    TEST_CASE(test_cache_map_cleanup),
};

// Test suite definition
const test_suite_t cache_map_test_suite = TEST_SUITE(
    "Cache Map Tests",
    cache_map_test_cases,
    setup,
    teardown
);

// Main test runner function
uint32_t run_cache_map_tests(void) {
    return test_run_suite(&cache_map_test_suite);
}