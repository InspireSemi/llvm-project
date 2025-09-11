/**
 * @file device_memory.c
 * @brief Device memory abstraction implementation
 */

#include "device_memory.h"
#include "resource_map.h"
#include "lle_manager.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/llext/symbol.h>

// Include shared memory backend header
#include TX_SHARED_MEMORY_BACKEND_HEADER

LOG_MODULE_REGISTER(device_memory, LOG_LEVEL_DBG);

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
        LOG_DBG("Added shared memory allocation to resource map: %p", ptr);
        if (result != 0) {
            LOG_WRN("Failed to add shared memory allocation to resource map: %p", ptr);
        }
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
    
    // Remove from resource map and free
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
