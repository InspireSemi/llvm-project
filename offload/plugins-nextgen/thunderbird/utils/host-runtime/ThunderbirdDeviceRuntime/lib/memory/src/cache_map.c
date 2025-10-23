#include "cache_map.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_map.h>
#include <zephyr/sys/hash_function.h>
#include <zephyr/llext/symbol.h>

#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(cache_map, LOG_LEVEL_DBG);

// Define hashmaps - they are automatically initialized
SYS_HASHMAP_DEFINE_STATIC(forward_map);
SYS_HASHMAP_DEFINE_STATIC(reverse_map);

typedef struct {
    struct sys_hashmap *forward_map;   // shared_address -> cache_pair_t*
    struct sys_hashmap *reverse_map;   // exclusive_address -> cache_pair_t*
    struct k_mutex mutex;              // For protecting write operations
} cache_map_t;

static cache_map_t g_cache_map;

int cache_map_init(void) {
    k_mutex_init(&g_cache_map.mutex);
    
    g_cache_map.forward_map = &forward_map;
    g_cache_map.reverse_map = &reverse_map;
    
    return 0;
}

int cache_map_add_pair(uintptr_t shared_addr, size_t shared_size, 
                       uintptr_t exclusive_addr, size_t exclusive_size) {
    // Input validation
    if (shared_addr == 0 || exclusive_addr == 0 || 
        shared_size == 0 || exclusive_size == 0) {
        return -EINVAL;
    }
    
    k_mutex_lock(&g_cache_map.mutex, K_FOREVER);
    
    LOG_DBG("Adding pair: shared=0x%lx (size=%zu), exclusive=0x%lx (size=%zu)", 
            shared_addr, shared_size, exclusive_addr, exclusive_size);
    
    // Check if addresses already exist
    uint64_t existing_value;
    if (sys_hashmap_get(g_cache_map.forward_map, (uint64_t)shared_addr, &existing_value)) {
        k_mutex_unlock(&g_cache_map.mutex);
        return -EEXIST;  // Shared address already exists
    }
    if (sys_hashmap_get(g_cache_map.reverse_map, (uint64_t)exclusive_addr, &existing_value)) {
        k_mutex_unlock(&g_cache_map.mutex);
        return -EEXIST;  // Exclusive address already exists
    }
    
    cache_pair_t *pair = k_malloc(sizeof(cache_pair_t));
    if (!pair) {
        k_mutex_unlock(&g_cache_map.mutex);
        return -ENOMEM;
    }
    
    pair->shared_address = shared_addr;
    pair->shared_size = shared_size;
    pair->exclusive_address = exclusive_addr;
    pair->exclusive_size = exclusive_size;
    atomic_set(&pair->initial_sync_performed, 0);
    atomic_set(&pair->exclusive_is_dirty, 0);
    
    // sys_hashmap_insert returns 1 on success, 0 if replaced, <0 on error
    int ret = sys_hashmap_insert(g_cache_map.forward_map, 
                                  (uint64_t)shared_addr,
                                  (uint64_t)pair,
                                  NULL);
    if (ret < 0) {
        k_free(pair);
        k_mutex_unlock(&g_cache_map.mutex);
        return ret;
    }
    
    ret = sys_hashmap_insert(g_cache_map.reverse_map, 
                             (uint64_t)exclusive_addr,
                             (uint64_t)pair,
                             NULL);
    if (ret < 0) {
        sys_hashmap_remove(g_cache_map.forward_map, (uint64_t)shared_addr, NULL);
        k_free(pair);
        k_mutex_unlock(&g_cache_map.mutex);
        return ret;
    }
    
    LOG_DBG("Successfully added pair with %zu total entries", 
            sys_hashmap_size(g_cache_map.forward_map));
    k_mutex_unlock(&g_cache_map.mutex);
    return 0;
}

int cache_map_remove_pair(uintptr_t shared_addr) {
    LOG_DBG("Removing pair for shared address 0x%lx", shared_addr);
    
    k_mutex_lock(&g_cache_map.mutex, K_FOREVER);
    
    uint64_t value;
    // sys_hashmap_remove returns bool - true if found and removed
    bool found = sys_hashmap_remove(g_cache_map.forward_map, 
                                    (uint64_t)shared_addr,
                                    &value);
    
    if (!found) {
        LOG_WRN("Attempted to remove non-existent shared address 0x%lx", shared_addr);
        k_mutex_unlock(&g_cache_map.mutex);
        return -ENOENT;
    }
    
    cache_pair_t *pair = (cache_pair_t *)value;
    sys_hashmap_remove(g_cache_map.reverse_map, 
                       (uint64_t)pair->exclusive_address,
                       NULL);
    k_free(pair);
    
    LOG_DBG("Successfully removed pair, %zu entries remaining", 
            sys_hashmap_size(g_cache_map.forward_map));
    k_mutex_unlock(&g_cache_map.mutex);
    return 0;
}

cache_pair_t* cache_map_find_by_shared(uintptr_t shared_addr) {
    k_mutex_lock(&g_cache_map.mutex, K_FOREVER);
    uint64_t value;
    bool found = sys_hashmap_get(g_cache_map.forward_map, 
                                 (uint64_t)shared_addr,
                                 &value);
    k_mutex_unlock(&g_cache_map.mutex);
    return found ? (cache_pair_t *)value : NULL;
}
EXPORT_SYMBOL(cache_map_find_by_shared);

cache_pair_t* cache_map_find_by_exclusive(uintptr_t exclusive_addr) {
    k_mutex_lock(&g_cache_map.mutex, K_FOREVER);
    uint64_t value;
    bool found = sys_hashmap_get(g_cache_map.reverse_map, 
                                 (uint64_t)exclusive_addr,
                                 &value);
    k_mutex_unlock(&g_cache_map.mutex);
    return found ? (cache_pair_t *)value : NULL;
}

uintptr_t cache_map_get_exclusive_from_shared(uintptr_t shared_addr) {
    cache_pair_t *pair = cache_map_find_by_shared(shared_addr);
    return pair ? pair->exclusive_address : 0;
}

bool do_we_need_to_sync(cache_pair_t *pair) {
    if (!pair) return false;
    return !atomic_get(&pair->initial_sync_performed) || 
           atomic_get(&pair->exclusive_is_dirty);
}

// Helper function for bounds checking
static inline bool is_valid_range(size_t offset, size_t chunk_size, 
                                  size_t num_chunks, size_t buffer_size) {
    // Handle zero cases
    if (chunk_size == 0 || num_chunks == 0) {
        return true;  // Zero-size operations are valid
    }
    
    // Check for overflow in multiplication
    if (num_chunks > (SIZE_MAX / chunk_size)) {
        return false;  // Would overflow
    }
    
    size_t total_size = chunk_size * num_chunks;
    
    // Check for overflow in addition and bounds
    if (offset > buffer_size) {
        return false;  // Offset beyond buffer
    }
    
    size_t remaining_size = buffer_size - offset;
    return total_size <= remaining_size;
}

int cache_map_sync_shared_to_exclusive(cache_pair_t *pair, size_t offset, 
                                       size_t chunk_size, size_t num_chunks) {
    if (!pair) {
        LOG_ERR("Invalid pair pointer");
        return -EINVAL;
    }
    
    // Add debug logging before bounds check
    size_t total_size = chunk_size * num_chunks;
    LOG_DBG("Bounds check: offset=%zu, chunk_size=%zu, num_chunks=%zu, total_size=%zu, "
            "shared_size=%zu, exclusive_size=%zu", 
            offset, chunk_size, num_chunks, total_size, pair->shared_size, pair->exclusive_size);
    
    if (!is_valid_range(offset, chunk_size, num_chunks, pair->shared_size) ||
        !is_valid_range(offset, chunk_size, num_chunks, pair->exclusive_size)) {
        LOG_ERR("Sync bounds check failed: offset=%zu, chunk_size=%zu, num_chunks=%zu, "
                "shared_size=%zu, exclusive_size=%zu", 
                offset, chunk_size, num_chunks, pair->shared_size, pair->exclusive_size);
        return -EINVAL;
    }
    
    LOG_DBG("Syncing %zu bytes from shared(0x%lx+%zu) to exclusive(0x%lx+%zu)",
            total_size, pair->shared_address, offset, pair->exclusive_address, offset);
    
    memcpy((uint8_t *)pair->exclusive_address + offset, 
           (uint8_t *)pair->shared_address + offset, total_size);
    atomic_set(&pair->initial_sync_performed, 1);
    atomic_set(&pair->exclusive_is_dirty, 0);
    return 0;
}
EXPORT_SYMBOL(cache_map_sync_shared_to_exclusive);

int cache_map_sync_exclusive_to_shared(cache_pair_t *pair, size_t offset, 
                                       size_t chunk_size, size_t num_chunks) {
    if (!pair) {
        LOG_ERR("Invalid pair pointer");
        return -EINVAL;
    }
    
    if (!is_valid_range(offset, chunk_size, num_chunks, pair->shared_size) ||
        !is_valid_range(offset, chunk_size, num_chunks, pair->exclusive_size)) {
        LOG_ERR("Sync bounds check failed: offset=%zu, chunk_size=%zu, num_chunks=%zu, "
                "shared_size=%zu, exclusive_size=%zu", 
                offset, chunk_size, num_chunks, pair->shared_size, pair->exclusive_size);
        return -EINVAL;
    }
    
    size_t total_size = chunk_size * num_chunks;
    LOG_DBG("Syncing %zu bytes from exclusive(0x%lx+%zu) to shared(0x%lx+%zu)",
            total_size, pair->exclusive_address, offset, pair->shared_address, offset);
    
    memcpy((uint8_t *)pair->shared_address + offset, 
           (uint8_t *)pair->exclusive_address + offset, total_size);
    atomic_set(&pair->exclusive_is_dirty, 0);
    return 0;
}
EXPORT_SYMBOL(cache_map_sync_exclusive_to_shared);

int cache_map_set_status(cache_pair_t *pair, bool initial_sync_performed, 
                        bool exclusive_is_dirty) {
    if (!pair) return -EINVAL;
    atomic_set(&pair->initial_sync_performed, initial_sync_performed ? 1 : 0);
    atomic_set(&pair->exclusive_is_dirty, exclusive_is_dirty ? 1 : 0);
    return 0;
}
EXPORT_SYMBOL(cache_map_set_status);

int cache_map_set_status_by_exclusive(uintptr_t exclusive_addr, 
                                      bool initial_sync_performed, 
                                      bool exclusive_is_dirty) {
    cache_pair_t *pair = cache_map_find_by_exclusive(exclusive_addr);
    if (!pair) return -ENOENT;
    return cache_map_set_status(pair, initial_sync_performed, exclusive_is_dirty);
}

size_t cache_map_get_count(void) {
    k_mutex_lock(&g_cache_map.mutex, K_FOREVER);
    size_t count = sys_hashmap_size(g_cache_map.forward_map);
    k_mutex_unlock(&g_cache_map.mutex);
    return count;
}

static void cleanup_callback(uint64_t key, uint64_t value, void *cookie) {
    (void)key;
    (void)cookie;
    cache_pair_t *pair = (cache_pair_t *)value;
    k_free(pair);
}

void cache_map_cleanup(void) {
    LOG_INF("Starting cache map cleanup");
    
    k_mutex_lock(&g_cache_map.mutex, K_FOREVER);
    
    size_t count_before = sys_hashmap_size(g_cache_map.forward_map);
    
    // Clear and free all pairs
    sys_hashmap_clear(g_cache_map.forward_map, cleanup_callback, NULL);
    sys_hashmap_clear(g_cache_map.reverse_map, NULL, NULL);
    
    k_mutex_unlock(&g_cache_map.mutex);
    
    LOG_INF("Cache map cleanup complete, freed %zu pairs", count_before);
}
