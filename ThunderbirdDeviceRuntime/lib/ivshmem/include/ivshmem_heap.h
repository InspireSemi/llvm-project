/**
 * @file ivshmem_heap.h
 * @brief IVSHMEM shared memory heap management interface
 * @details Provides thread-safe dynamic memory allocation within the IVSHMEM
 *          shared memory region using Zephyr's k_heap. Includes convenience
 *          wrappers for malloc/free operations with timeout support.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#include <zephyr/kernel.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Get the internal k_heap handle for IVSHMEM memory
 * @return Pointer to the k_heap structure if you want to pass it into APIs
 */
struct k_heap *ivshmem_heap_get(void);

/**
 * @brief Query the total size of the IVSHMEM heap
 * @return Heap size in bytes
 */
size_t ivshmem_heap_bytes(void);


/**
 * @brief Allocate memory from the IVSHMEM heap
 * @param bytes Number of bytes to allocate
 * @return Pointer to allocated memory on success, NULL on failure
 */
void *ivshmem_malloc(size_t bytes);

/**
 * @brief Free memory previously allocated from the IVSHMEM heap
 * @param ptr Pointer to memory to free (NULL is safely ignored)
 */
void ivshmem_free(void *ptr);
