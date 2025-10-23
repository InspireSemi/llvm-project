#pragma once

/**
 * @file tx_media_dependent_defines.h
 * @brief Transmission media dependent defines and aliases
 * @details Provides generic aliases for transmission media specific constants,
 *          allowing library code to be independent of the underlying transport
 *          implementation (IVSHMEM, TCP, etc.)
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

// Include the actual implementation-specific defines
#include "ivshmem_abi.h"
#include "ivshmem_dt.h"

/**
 * @defgroup tx_media_aliases Transmission Media Generic Aliases
 * @brief Generic aliases for transmission media specific constants
 * @{
 */

// Shared memory base and sizing aliases
#define TX_SHM_BASE_ADDR        IVSHMEM_SHM_BASE
#define TX_SHM_TOTAL_SIZE       IVSHMEM_SHM_SIZE
#define TX_CTRL_REGION_SIZE     IVSHMEM_CTRL_REGION_MAX_SIZE

// Mailbox positioning aliases
#define TX_MBOX_H2D_OFFSET      MBOX_H2D_OFFSET
#define TX_MBOX_D2H_OFFSET      MBOX_D2H_OFFSET

// Calculated mailbox addresses (generic)
#define TX_MBOX_H2D_ADDR        (TX_SHM_BASE_ADDR + TX_MBOX_H2D_OFFSET)
#define TX_MBOX_D2H_ADDR        (TX_SHM_BASE_ADDR + TX_MBOX_D2H_OFFSET)

/**
 * @brief Get the generic H2D mailbox base address
 * @return Base address for host-to-device mailbox
 */
static inline uintptr_t tx_get_h2d_mailbox_addr(void)
{
    return TX_MBOX_H2D_ADDR;
}

/**
 * @brief Get the generic D2H mailbox base address  
 * @return Base address for device-to-host mailbox
 */
static inline uintptr_t tx_get_d2h_mailbox_addr(void)
{
    return TX_MBOX_D2H_ADDR;
}

/**
 * @brief Get the H2D mailbox offset from shared memory base
 * @return Offset in bytes
 */
static inline size_t tx_get_h2d_offset(void)
{
    return TX_MBOX_H2D_OFFSET;
}

/**
 * @brief Get the D2H mailbox offset from shared memory base
 * @return Offset in bytes
 */
static inline size_t tx_get_d2h_offset(void)
{
    return TX_MBOX_D2H_OFFSET;
}

/**
 * @brief Get the shared memory base address
 * @return Base address of shared memory region
 */
static inline uintptr_t tx_get_shm_base_addr(void)
{
    return TX_SHM_BASE_ADDR;
}

/**
 * @brief Get the total shared memory size
 * @return Total size of shared memory region in bytes
 */
static inline size_t tx_get_shm_total_size(void)
{
    return TX_SHM_TOTAL_SIZE;
}

/** @} */ // end of tx_media_aliases group

/**
 * @defgroup memory_backend_config Memory Backend Configuration
 * @brief Configuration for shared memory backend implementation
 * @{
 */

// Shared memory backend configuration
#define TX_SHARED_MEMORY_BACKEND_HEADER    "ivshmem_heap.h"
#define TX_SHARED_MALLOC                   ivshmem_malloc
#define TX_SHARED_FREE                     ivshmem_free
#define TX_SHARED_HEAP_BYTES               ivshmem_heap_bytes
#define TX_SHARED_INIT()                  (true)
#define TX_SHARED_DEINIT()                 // No-op for ivshmem_heap

/** @} */ // end of memory_backend_config group
