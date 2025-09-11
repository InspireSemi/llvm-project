#include "resource_map.h"
#include "device_memory.h"
#include "lle_manager.h"  // For LLE manager API
#include <string.h>
#include <zephyr/kernel.h>  // For k_malloc and k_free
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(resource_map, CONFIG_LOG_DEFAULT_LEVEL);

#define INITIAL_CAPACITY 16
#define GROWTH_FACTOR 2

typedef struct {
    resource_entry_t *entries;
    size_t count;
    size_t capacity;
} resource_map_t;

// Static internal map
static resource_map_t g_resource_map = {0};

int resource_map_init(void)
{
    LOG_DBG("Initializing resource map with capacity %d", INITIAL_CAPACITY);
    LOG_DBG("Current state before init: entries=%p, capacity=%zu, count=%zu", 
            g_resource_map.entries, g_resource_map.capacity, g_resource_map.count);
    
    // If already allocated, cleanup and reset to clean state
    if (g_resource_map.entries != NULL) {
        LOG_DBG("Resource map already allocated, cleaning up existing state");
        LOG_DBG("Before cleanup: count=%zu", g_resource_map.count);
        resource_map_cleanup();
        LOG_DBG("After cleanup: count=%zu", g_resource_map.count);
        
        LOG_DBG("Resource map reset, capacity=%zu, count=%zu", g_resource_map.capacity, g_resource_map.count);
        return 0;
    }
    
    // First time initialization - allocate the array
    size_t alloc_size = INITIAL_CAPACITY * sizeof(resource_entry_t);
    LOG_DBG("First initialization: allocating %zu bytes", alloc_size);
    
    g_resource_map.entries = (resource_entry_t *)k_malloc(alloc_size);
    if (!g_resource_map.entries) {
        LOG_ERR("Failed to allocate %zu bytes for resource map entries", alloc_size);
        return -1;
    }
    
    g_resource_map.count = 0;
    g_resource_map.capacity = INITIAL_CAPACITY;
    LOG_DBG("Resource map initialized: entries=%p, capacity=%zu", 
            g_resource_map.entries, g_resource_map.capacity);
    return 0;
}

static int resource_map_grow(void)
{
    size_t new_capacity = g_resource_map.capacity * GROWTH_FACTOR;
    size_t new_size = new_capacity * sizeof(resource_entry_t);
    size_t old_size = g_resource_map.capacity * sizeof(resource_entry_t);
    
    // Allocate new memory - use k_malloc directly for resource map internal storage
    resource_entry_t *new_entries = (resource_entry_t *)k_malloc(new_size);
    if (!new_entries) {
        return -1;
    }
    
    // Copy existing data
    memcpy(new_entries, g_resource_map.entries, old_size);
    
    // Free old memory - use k_free directly for resource map internal storage
    k_free(g_resource_map.entries);
    
    // Update map
    g_resource_map.entries = new_entries;
    g_resource_map.capacity = new_capacity;
    return 0;
}

static int find_index(uintptr_t address)
{
    // Linear search for now - could be binary search if kept sorted
    LOG_DBG("find_index: searching for 0x%lx, count=%zu", address, g_resource_map.count);
    for (size_t i = 0; i < g_resource_map.count; i++) {
        if (g_resource_map.entries[i].address == address) {
            LOG_DBG("find_index: FOUND at index %zu", i);
            return (int)i;
        }
    }
    LOG_DBG("find_index: NOT FOUND, returning -1");
    return -1;
}

int resource_map_add(uintptr_t address, size_t size, bool is_shared_memory)
{
    LOG_DBG("resource_map_add called: address=0x%lx, size=%zu, count=%zu", 
            address, size, g_resource_map.count);
    
    if (size == 0) {
        LOG_ERR("Invalid size 0 for resource map entry");
        return -1;
    }
    
    // Check if not initialized
    if (!g_resource_map.entries) {
        LOG_ERR("Resource map not initialized - call resource_map_init() first");
        return -1;
    }
    
    // Check if address already exists
    int existing_index = find_index(address);
    LOG_DBG("find_index(0x%lx) returned %d", address, existing_index);
    if (existing_index != -1) {
        LOG_ERR("Address 0x%lx already exists in resource map", address);
        return -1;
    }
    
    // Grow array if needed
    if (g_resource_map.count >= g_resource_map.capacity) {
        if (resource_map_grow() != 0) {
            return -1;
        }
    }
    
    // Add new entry
    resource_entry_t *entry = &g_resource_map.entries[g_resource_map.count];
    entry->address = address;
    entry->size = size;
    entry->is_lle_module = false;
    entry->is_loaded = false;
    entry->is_shared_memory = is_shared_memory;
    
    g_resource_map.count++;
    LOG_DBG("Added entry at index %zu, new count=%zu", g_resource_map.count - 1, g_resource_map.count);
    return 0;
}

int resource_map_remove(uintptr_t address)
{
    if (g_resource_map.count == 0 || !g_resource_map.entries) {
        return -1;
    }
    
    int index = find_index(address);
    if (index == -1) {
        return -1;
    }
    
    // Move last element to fill the gap (unordered removal)
    if (index != (int)(g_resource_map.count - 1)) {
        g_resource_map.entries[index] = g_resource_map.entries[g_resource_map.count - 1];
    }
    
    g_resource_map.count--;
    return 0;
}

resource_entry_t* resource_map_find(uintptr_t address)
{
    if (!g_resource_map.entries) {
        return NULL;
    }
    
    int index = find_index(address);
    if (index == -1) {
        return NULL;
    }
    
    return &g_resource_map.entries[index];
}

int resource_map_set_lle_status(uintptr_t address, bool is_lle_module)
{
    resource_entry_t *entry = resource_map_find(address);
    if (!entry) {
        return -1;
    }
    
    entry->is_lle_module = is_lle_module;
    return 0;
}

int resource_map_set_loaded_status(uintptr_t address, bool is_loaded)
{
    resource_entry_t *entry = resource_map_find(address);
    if (!entry) {
        return -1;
    }
    
    entry->is_loaded = is_loaded;
    return 0;
}

int resource_map_set_llext_handle(uintptr_t address, struct llext *handle)
{
    resource_entry_t *entry = resource_map_find(address);
    if (!entry) {
        return -1;
    }
    
    entry->llext_handle = handle;
    return 0;
}

struct llext* resource_map_get_llext_handle(uintptr_t address)
{
    resource_entry_t *entry = resource_map_find(address);
    if (!entry) {
        return NULL;
    }
    
    return entry->llext_handle;
}

size_t resource_map_get_count(void)
{
    return g_resource_map.count;
}

void resource_map_cleanup(void)
{
    LOG_DBG("Cleaning up resource map");
    
    if (g_resource_map.entries == NULL) {
        LOG_WRN("Resource map cleanup called but entries is NULL");
        return;
    }
    
    LOG_DBG("Before cleanup: count=%zu", g_resource_map.count);
    
    // Unload any LLE modules
    for (size_t i = 0; i < g_resource_map.count; i++) {
        resource_entry_t *entry = &g_resource_map.entries[i];
        LOG_DBG("Cleanup: unloading entry %zu at address 0x%lx", i, entry->address);
        bool unloaded = lle_manager_unload_if_loaded(entry->address);
        if (unloaded) {
            LOG_DBG("Unloaded LLE module at address 0x%lx", entry->address);
        }
    }
    
    // Reset count - this should be sufficient since find_index only searches up to count
    g_resource_map.count = 0;
    
    LOG_DBG("After cleanup: count=%zu", g_resource_map.count);
    LOG_DBG("Resource map cleaned up, count reset to 0, entries array preserved");
}