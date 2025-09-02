/**
 * @file ivshmem_shm.c
 * @brief IVSHMEM shared memory pointer definition
 * @details Defines the global shared memory pointer that provides typed access
 *          to the IVSHMEM region. The pointer is initialized at compile time
 *          with the base address from device tree configuration.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "../include/ivshmem_shm.h"
#include "../include/ivshmem_dt.h"

// Define the shared memory pointer
volatile uint8_t (*const ivshmem_shm)[IVSHMEM_SHM_SIZE] =
    (volatile uint8_t (*)[IVSHMEM_SHM_SIZE])(uintptr_t)IVSHMEM_SHM_BASE;

/**
 * @brief Get base address of IVSHMEM shared memory
 * @return Base address of shared memory region
 */
uintptr_t ivshmem_get_base_address(void)
{
    return IVSHMEM_SHM_BASE;
}

/**
 * @brief Get size of IVSHMEM shared memory
 * @return Size of shared memory region in bytes
 */
size_t ivshmem_get_size(void)
{
    return IVSHMEM_SHM_SIZE;
}

// Convenience functions
void *ivshmem_shm_ptr(void)
{
    return (void *)ivshmem_shm;
}

size_t ivshmem_shm_size(void)
{
    return IVSHMEM_SHM_SIZE;
}
