/**
 * @file ivshmem_heap.c
 * @brief IVSHMEM shared memory heap management implementation
 * @details Provides dynamic memory allocation within the IVSHMEM shared memory region.
 *          Uses k_heap for thread-safe memory management starting at a configurable
 *          offset within the shared memory space. Initializes automatically via SYS_INIT.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <inttypes.h>
#include <zephyr/sys/sys_heap.h>
#include "../include/ivshmem_dt.h"

LOG_MODULE_REGISTER(ivshmem_heap, CONFIG_LOG_DEFAULT_LEVEL);

/* Default heap offset: 64 KiB if property absent */

BUILD_ASSERT(HEAP_OFFSET < IVSHMEM_SHM_SIZE, "Heap offset must be < shared memory size");

static struct k_heap g_heap;
static size_t g_heap_size;

/**
 * @brief System initialization function for IVSHMEM heap
 * @return 0 on success, negative error code on failure
 * @retval -EINVAL if heap size is too small after alignment
 */
static int ivshmem_heap_sys_init(void)
{
    uintptr_t shm_base = (uintptr_t)IVSHMEM_SHM_BASE;
    uintptr_t base = shm_base + (uintptr_t)HEAP_OFFSET;

    /* Align base up and size down to HEAP_ALIGN */
    uintptr_t aligned_base = ROUND_UP(base, HEAP_ALIGN);
    size_t avail = IVSHMEM_SHM_SIZE - (aligned_base - shm_base);
    size_t aligned_size = ROUND_DOWN(avail, HEAP_ALIGN);

    if (aligned_size < 1024) {
        LOG_ERR("IVSHMEM heap too small: %zu bytes", aligned_size);
        return -EINVAL;
    }

    k_heap_init(&g_heap, (void *)aligned_base, aligned_size);
    g_heap_size = aligned_size;

    LOG_INF("IVSHMEM k_heap: base=0x%0" PRIxPTR " size=%zu (offset=0x%08x)",
        aligned_base, g_heap_size, (uint32_t)HEAP_OFFSET);
    return 0;
}
SYS_INIT(ivshmem_heap_sys_init, APPLICATION, 50);

/**
 * @brief Get the internal k_heap handle for IVSHMEM memory
 * @return Pointer to the k_heap structure for IVSHMEM shared memory
 */
struct k_heap *ivshmem_heap_get(void)
{
    return &g_heap;
}


/**
 * @brief Get the total size of the IVSHMEM heap in bytes
 * @return Size of the heap in bytes
 */
size_t ivshmem_heap_bytes(void)
{
	// struct sys_memory_stats stats;
	// sys_heap_runtime_stats_get(&(g_heap.heap), &stats);
    // return stats.allocated_bytes + stats.free_bytes;
    return 0; // TODO: Re-enable when heap stats API is available
}


/**
 * @brief Allocate memory from the IVSHMEM heap
 * @param bytes Number of bytes to allocate
 * @return Pointer to allocated memory on success, NULL on failure
 */
void *ivshmem_malloc(size_t bytes)
{
    if (bytes == 0) {
        return NULL;
    }
    return k_heap_alloc(&g_heap, bytes, K_NO_WAIT);
}


/**
 * @brief Free memory previously allocated from the IVSHMEM heap
 * @param ptr Pointer to memory to free (NULL is safely ignored)
 */
void ivshmem_free(void *ptr)
{
    if (!ptr) {
        return;
    }
    k_heap_free(&g_heap, ptr);
}