#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration for LLEXT structure
struct llext;

typedef struct {
    uintptr_t address;
    size_t size;
    bool is_lle_module;
    bool is_loaded;
    bool is_shared_memory;
    struct llext *llext_handle;  // Handle to loaded LLEXT module
} resource_entry_t;

/**
 * Helper function
 */
resource_entry_t* resource_map_find_containing(uintptr_t address);

/**
 * Initialize the resource map
 */
int resource_map_init(void);

/**
 * Add a memory allocation record to the map
 */
int resource_map_add(uintptr_t address, size_t size, bool is_shared_memory);

/**
 * Remove a memory allocation record from the map
 */
int resource_map_remove(uintptr_t address);

/**
 * Find a resource entry by address
 */
resource_entry_t* resource_map_find(uintptr_t address);

/**
 * Set LLE module status for a resource
 */
int resource_map_set_lle_status(uintptr_t address, bool is_lle_module);

/**
 * Set loaded status for a resource
 */
int resource_map_set_loaded_status(uintptr_t address, bool is_loaded);

/**
 * Set LLEXT handle for a resource
 */
int resource_map_set_llext_handle(uintptr_t address, struct llext *handle);

/**
 * Get LLEXT handle for a resource
 */
struct llext* resource_map_get_llext_handle(uintptr_t address);

/**
 * Get the total number of entries in the map
 */
size_t resource_map_get_count(void);

/**
 * Clean up and free all resources in the map
 */
void resource_map_cleanup(void);
