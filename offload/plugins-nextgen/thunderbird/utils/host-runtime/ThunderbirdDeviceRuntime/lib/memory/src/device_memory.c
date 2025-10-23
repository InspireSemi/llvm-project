/**
 * @file device_memory.c
 * @brief Device memory abstraction implementation
 */

#define USE_CACHE_MAP 0 // This is only applicable to A1 revision hardware, nothing earlier, nothing later, so disable by default

#include "device_memory.h"
#include "resource_map.h"
#include "lle_manager.h"
#ifdef USE_CACHE_MAP
#include "cache_map.h"  // Added for cache map integration
#endif
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/llext/symbol.h>

// Include shared memory backend header
#include TX_SHARED_MEMORY_BACKEND_HEADER

LOG_MODULE_REGISTER(device_memory, LOG_LEVEL_DBG);

// Define macro to enable cache map integration

static bool memory_initialized = false;

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Check if a pointer is within the shared memory region
 * @param ptr Pointer to check
 * @return true if pointer is in shared memory region, false otherwise
 */
static inline bool is_shared_memory_ptr(const void *ptr)
{
    if (ptr == NULL) {
        return false;
    }
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t shm_start = tx_get_shm_base_addr();
    uintptr_t shm_end = shm_start + tx_get_shm_total_size();
    
    return (addr >= shm_start && addr < shm_end);
}

// =============================================================================
// Initialization and Management
// =============================================================================

int device_memory_init(void)
{
    if (memory_initialized) {
        return 0;
    }
    
    LOG_DBG("Initializing device memory subsystems");
    
    // Initialize resource map first
    int result = resource_map_init();
    if (result != 0) {
        LOG_ERR("Failed to initialize resource map");
        return -1;
    }
    
    // Initialize shared memory backend
    bool shm_result = TX_SHARED_INIT();
    if (!shm_result) {
        LOG_ERR("Failed to initialize shared memory backend");
        resource_map_cleanup();
        return -1;
    }
    
#ifdef USE_CACHE_MAP
    // Initialize cache map
    cache_map_init();
#endif
    
    // Exclusive memory (Zephyr heap) needs no initialization
    
    memory_initialized = true;
    LOG_DBG("Device memory subsystems initialized successfully");
    return 0;
}
EXPORT_SYMBOL(device_memory_init);

void device_memory_deinit(void)
{
    LOG_DBG("Deinitializing device memory subsystems");
    TX_SHARED_DEINIT();
    resource_map_cleanup();
#ifdef USE_CACHE_MAP
    cache_map_cleanup();  // Added to clean up cache map
#endif
    memory_initialized = false;
    // Exclusive memory (Zephyr heap) needs no deinitialization
}

// =============================================================================
// Shared Memory Functions
// =============================================================================

void *device_shared_malloc(size_t size)
{
    if(size < 1) {
        return NULL;
    }

    if (!memory_initialized) {
        LOG_ERR("Device memory not initialized");
        return NULL;
    }
    
    void *ptr = TX_SHARED_MALLOC(size);
    if (ptr != NULL) {
        // Add to resource map as shared memory
        LOG_DBG("Adding shared memory allocation to resource map: %p", ptr);
        int result = resource_map_add((uintptr_t)ptr, size, true);
        if (result != 0) {
            LOG_WRN("Failed to add shared memory allocation to resource map: %p", ptr);
        }
        
#ifdef USE_CACHE_MAP
        // Allocate paired exclusive memory using device_exclusive_malloc
        void *exclusive_ptr = device_exclusive_malloc(size);
        if (exclusive_ptr != NULL) {
            // Add pair to cache map
            int cache_result = cache_map_add_pair((uintptr_t)ptr, size, (uintptr_t)exclusive_ptr, size);
            if (cache_result != 0) {
                LOG_WRN("Failed to add pair to cache map: %p -> %p", ptr, exclusive_ptr);
                // Free exclusive memory if cache map add fails
                device_exclusive_free(exclusive_ptr);
            } else {
                LOG_DBG("Paired shared %p with exclusive %p in cache map", ptr, exclusive_ptr);
            }
        } else {
            LOG_ERR("Failed to allocate paired exclusive memory for shared %p", ptr);
            // Free shared memory and remove from resource map if exclusive allocation fails
            resource_map_remove((uintptr_t)ptr);
            TX_SHARED_FREE(ptr);
            return NULL;
        }
#endif
    }
    
    LOG_DBG("Shared malloc: %zu bytes at %p", size, ptr);
    return ptr;
}
EXPORT_SYMBOL(device_shared_malloc);

void device_shared_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    
    if (!memory_initialized) {
        LOG_WRN("Attempting to free shared memory before initialization");
        return;
    }
    
    // Validate that this pointer is actually from shared memory
    if (!is_shared_memory_ptr(ptr)) {
        LOG_ERR("Attempted to free non-shared memory pointer %p", ptr);
        return;
    }
    
    uintptr_t address = (uintptr_t)ptr;
    
    // Check if this address has a loaded LLE module and unload it first
    lle_manager_unload_if_loaded(address);
    
#ifdef USE_CACHE_MAP
    // Find and free the paired exclusive memory
    cache_pair_t *pair = cache_map_find_by_shared(address);
    if (pair != NULL) {
        LOG_DBG("Freeing paired exclusive memory %p for shared %p", (void *)pair->exclusive_address, ptr);
        device_exclusive_free((void *)pair->exclusive_address);
        // Remove pair from cache map
        cache_map_remove_pair(address);
    } else {
        LOG_WRN("No cache map pair found for shared memory %p", ptr);
    }
#endif
    
    // Remove from resource map and free shared memory
    resource_map_remove(address);
    TX_SHARED_FREE(ptr);
    
    LOG_DBG("Shared free: %p", ptr);
}
EXPORT_SYMBOL(device_shared_free);

// =============================================================================
// Exclusive Memory Functions
// =============================================================================

void *device_exclusive_malloc(size_t size)
{
    if(size < 1) {
        return NULL;
    }

    if (!memory_initialized) {
        LOG_ERR("Device memory not initialized");
        return NULL;
    }

    void *ptr = k_malloc(size);
    if (ptr != NULL) {
        // Add to resource map as exclusive memory
        LOG_DBG("Adding exclusive memory allocation to resource map: %p", ptr);
        int result = resource_map_add((uintptr_t)ptr, size, false);
        if (result != 0) {
            LOG_WRN("Failed to add exclusive memory allocation to resource map: %p", ptr);
        }
    }
    
    LOG_DBG("Exclusive malloc: %zu bytes at %p", size, ptr);
    return ptr;
}
EXPORT_SYMBOL(device_exclusive_malloc);

void device_exclusive_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    
    if (!memory_initialized) {
        LOG_WRN("Attempting to free exclusive memory before initialization");
        return;
    }

    if (is_shared_memory_ptr(ptr)) {
        LOG_ERR("Attempted to free non-exclusive memory pointer %p", ptr);
        return;
    }
    
    uintptr_t address = (uintptr_t)ptr;
    
    // Check if this address has a loaded LLE module and unload it first
    lle_manager_unload_if_loaded(address);
    
    // Remove from resource map and free
    resource_map_remove(address);
    k_free(ptr);
    
    LOG_DBG("Exclusive free: %p", ptr);
}
EXPORT_SYMBOL(device_exclusive_free);

size_t device_shared_total_size(void)
{
    if (!memory_initialized) {
        return 0;
    }
    
    return TX_SHARED_HEAP_BYTES();
}
