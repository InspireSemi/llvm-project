#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/sys/atomic.h>  // For atomic_t

/**
 * @brief Structure representing a cache pair: shared and exclusive memory regions.
 */
typedef struct {
    uintptr_t shared_address;           /**< Address of the shared memory buffer */
    size_t shared_size;                 /**< Size of the shared memory buffer */
    uintptr_t exclusive_address;        /**< Address of the exclusive memory buffer */
    size_t exclusive_size;              /**< Size of the exclusive memory buffer */
    atomic_t initial_sync_performed;    /**< Atomic flag: 1 if initial sync from shared to exclusive has been performed, 0 otherwise */
    atomic_t exclusive_is_dirty;        /**< Atomic flag: 1 if exclusive buffer has been modified since last sync, 0 otherwise */
} cache_pair_t;

/**
 * @brief Initialize the cache map
 * @return 0 on success, negative on failure
 */
int cache_map_init(void);

/**
 * @brief Register a pair of addresses (shared and exclusive).
 * It is expected that the buffers identified by the arguments were actually allocated at this point.
 * Side effect: Initializes initial_sync_performed and exclusive_is_dirty to 0 atomically. Single-threaded access assumed.
 * @param shared_addr Address of the shared buffer
 * @param shared_size Size of the shared buffer
 * @param exclusive_addr Address of the exclusive buffer
 * @param exclusive_size Size of the exclusive buffer
 * @return 0 on success, negative on failure
 */
int cache_map_add_pair(uintptr_t shared_addr, size_t shared_size, uintptr_t exclusive_addr, size_t exclusive_size);

/**
 * @brief Unregister a pair by shared address. Single-threaded access assumed.
 * @param shared_addr Shared address of the pair to remove
 * @return 0 on success, negative if not found
 */
int cache_map_remove_pair(uintptr_t shared_addr);

/**
 * @brief Find a pair by shared address. Thread-safe (read-only).
 * @param shared_addr Shared address to search for
 * @return Pointer to the cache_pair_t if found, NULL otherwise
 */
cache_pair_t* cache_map_find_by_shared(uintptr_t shared_addr);

/**
 * @brief Find a pair by exclusive address. Thread-safe (read-only).
 * @param exclusive_addr Exclusive address to search for
 * @return Pointer to the cache_pair_t if found, NULL otherwise
 */
cache_pair_t* cache_map_find_by_exclusive(uintptr_t exclusive_addr);

/**
 * @brief Get the exclusive address corresponding to a shared address. Thread-safe (read-only).
 * @param shared_addr Shared address
 * @return Exclusive address if found, 0 otherwise
 */
uintptr_t cache_map_get_exclusive_from_shared(uintptr_t shared_addr);

/**
 * @brief Check if synchronization from shared to exclusive is needed for a pair.
 * Returns true if initial sync not performed or exclusive is dirty.
 * @param pair Pointer to the cache pair
 * @return true if sync is needed, false otherwise
 */
bool do_we_need_to_sync(cache_pair_t *pair);

/**
 * @brief Sync data from shared buffer to exclusive buffer.
 * Copies num_chunks of chunk_size bytes starting from offset in both buffers.
 * Side effect: Atomically sets initial_sync_performed to 1 and exclusive_is_dirty to 0. Safe for worker threads.
 * @param pair Pointer to the cache pair
 * @param offset Offset in bytes from the start of the buffers
 * @param chunk_size Size of each chunk in bytes
 * @param num_chunks Number of chunks to copy
 * @return 0 on success, negative on failure (e.g., bounds check)
 */
int cache_map_sync_shared_to_exclusive(cache_pair_t *pair, size_t offset, size_t chunk_size, size_t num_chunks);

/**
 * @brief Sync data from exclusive buffer to shared buffer.
 * Copies num_chunks of chunk_size bytes starting from offset in both buffers.
 * Side effect: Atomically sets exclusive_is_dirty to 0. Safe for worker threads.
 * @param pair Pointer to the cache pair
 * @param offset Offset in bytes from the start of the buffers
 * @param chunk_size Size of each chunk in bytes
 * @param num_chunks Number of chunks to copy
 * @return 0 on success, negative on failure (e.g., bounds check)
 */
int cache_map_sync_exclusive_to_shared(cache_pair_t *pair, size_t offset, size_t chunk_size, size_t num_chunks);

/**
 * @brief Update synchronization and dirty status for a pair (call after manual sync or modification).
 * Uses atomic CAS to set flags safely. Safe for worker threads.
 * @param pair Pointer to the cache pair
 * @param initial_sync_performed New value for initial_sync_performed
 * @param exclusive_is_dirty New value for exclusive_is_dirty
 * @return 0 on success, negative on failure
 */
int cache_map_set_status(cache_pair_t *pair, bool initial_sync_performed, bool exclusive_is_dirty);

/**
 * @brief Update synchronization and dirty status for a pair by exclusive address (call after modifying exclusive memory).
 * Uses atomic CAS to set flags safely. Safe for worker threads.
 * @param exclusive_addr Exclusive address of the pair
 * @param initial_sync_performed New value for initial_sync_performed
 * @param exclusive_is_dirty New value for exclusive_is_dirty
 * @return 0 on success, negative on failure
 */
int cache_map_set_status_by_exclusive(uintptr_t exclusive_addr, bool initial_sync_performed, bool exclusive_is_dirty);

/**
 * @brief Get the total number of pairs. Single-threaded access assumed.
 * @return Number of pairs in the map
 */
size_t cache_map_get_count(void);

/**
 * @brief Clean up and free all pairs. Single-threaded access assumed.
 */
void cache_map_cleanup(void);