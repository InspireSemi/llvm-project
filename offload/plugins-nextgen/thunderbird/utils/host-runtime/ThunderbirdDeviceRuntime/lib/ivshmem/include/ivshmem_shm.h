/**
 * @file ivshmem_shm.h
 * @brief IVSHMEM shared memory pointer interface
 * @details Provides typed access to the IVSHMEM shared memory region through
 *          a global pointer and convenience functions. The pointer is
 *          initialized at compile time from device tree configuration.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include "ivshmem_dt.h"
#include <stddef.h>
#include <stdint.h>

// Minimum required IVSHMEM size
#define IVSHMEM_MIN_SIZE (4 * 1024)  // 4KB minimum

// Shared memory pointer declaration
extern volatile uint8_t (*const ivshmem_shm)[IVSHMEM_SHM_SIZE];

/**
 * @brief Get base address of IVSHMEM shared memory
 * @return Base address of shared memory region
 */
uintptr_t ivshmem_get_base_address(void);

/**
 * @brief Get size of IVSHMEM shared memory
 * @return Size of shared memory region in bytes
 */
size_t ivshmem_get_size(void);

// Convenience functions
void *ivshmem_shm_ptr(void);

size_t ivshmem_shm_size(void);