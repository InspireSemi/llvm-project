/**
 * @file unit_ivshmem_shm.c
 * @brief Unit tests for IVSHMEM shared memory functionality
 */

#include "unit_test_framework.h"
#include "ivshmem_shm.h"
#include "ivshmem_dt.h"
#include <zephyr/kernel.h>

static test_result_t test_ivshmem_get_base(void)
{
    uintptr_t base = ivshmem_get_base_address();
    TEST_ASSERT_NEQ(0, base, "IVSHMEM base address should not be zero");
    TEST_ASSERT_EQ(IVSHMEM_SHM_BASE, base, "Base address should match expected value");
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_get_size(void)
{
    size_t size = ivshmem_get_size();
    TEST_ASSERT_POSITIVE(size, "IVSHMEM size should be positive");
    TEST_ASSERT_IN_RANGE(size, IVSHMEM_MIN_SIZE, SIZE_MAX, "Size should meet minimum requirements");
    TEST_PASS_RESULT();
}

static test_result_t test_ivshmem_memory_accessible(void)
{
    uintptr_t base = ivshmem_get_base_address();
    volatile uint32_t* test_ptr = (volatile uint32_t*)base;
    
    *test_ptr = 0xDEADBEEF;
    uint32_t read_value = *test_ptr;
    
    TEST_ASSERT_EQ(0xDEADBEEF, read_value, "Memory should be writable and readable");
    TEST_PASS_RESULT();
}

static const test_case_t ivshmem_shm_test_cases[] = {
    TEST_CASE(test_ivshmem_get_base),
    TEST_CASE(test_ivshmem_get_size),
    TEST_CASE(test_ivshmem_memory_accessible),
};

static const test_suite_t ivshmem_shm_test_suite = TEST_SUITE(
    "IVSHMEM Shared Memory Tests",
    ivshmem_shm_test_cases,
    NULL,
    NULL
);

uint32_t run_ivshmem_shm_unit_tests(void)
{
    return test_run_suite(&ivshmem_shm_test_suite);
}
