/**
 * @file device_memory.h
 * @brief Device memory abstraction layer
 * @details Provides dual memory allocation interfaces:
 *          - Shared memory: accessible by external host
 *          - Exclusive memory: device-only memory for temporary/internal use
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "tx_media_dependent_defines.h"

/**
 * @defgroup device_memory Device Memory Interface
 * @brief Dual memory allocation interface for shared and exclusive memory
 * @{
 */

// =============================================================================
// Initialization and Management
// =============================================================================

/**
 * @brief Initialize the device memory subsystem
 * @return 0 on success, negative error code on failure
 */
int device_memory_init(void);

/**
 * @brief Deinitialize the device memory subsystem
 */
void device_memory_deinit(void);

// =============================================================================
// Shared Memory Functions (Host-Accessible)
// =============================================================================

/**
 * @brief Allocate shared memory accessible by external host
 * @param size Size in bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *device_shared_malloc(size_t size);

/**
 * @brief Free previously allocated shared memory
 * @param ptr Pointer to memory to free
 * @note Will validate that ptr is from shared memory region
 */
void device_shared_free(void *ptr);

// =============================================================================
// Exclusive Memory Functions (Device-Only)
// =============================================================================

/**
 * @brief Allocate exclusive memory for device-only use
 * @param size Size in bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *device_exclusive_malloc(size_t size);

/**
 * @brief Free previously allocated exclusive memory
 * @param ptr Pointer to memory to free
 * @note Will validate that ptr is NOT from shared memory region
 */
void device_exclusive_free(void *ptr);

/**
 * @brief Get total size of device memory available
 * @return Total size in bytes (this is max size controlled by heap no the remaining capacity)
 * @note This includes both shared and exclusive memory regions
*/
size_t device_shared_total_size(void);

/**

*/
size_t device_exclusive_total_size(void);
