#include "lle_manager.h"
#include "resource_map.h"
#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lle_manager, LOG_LEVEL_DBG);

// Default configuration
#ifndef CONFIG_LLE_ENTRY_POINT_SYMBOL
#define CONFIG_LLE_ENTRY_POINT_SYMBOL "__omp_offloading_802_ec95c4_main_l11"
#endif

#ifndef CONFIG_LLE_NAME_PREFIX
#define CONFIG_LLE_NAME_PREFIX "kernel_"
#endif

// Load LLEXT module from memory buffer
static int lle_load_module(uintptr_t address, size_t size)
{
    LOG_DBG("Attempting to load LLEXT module at 0x%lx, size %zu", address, size);
    
    // Create buffer loader for the module at the given address
    struct llext_buf_loader buf_loader = LLEXT_BUF_LOADER((uint8_t*)address, size);
    struct llext_loader *ldr = &buf_loader.loader;
    
    struct llext_load_param ldr_param = LLEXT_LOAD_PARAM_DEFAULT;
    struct llext *ext;
    
    // Generate a unique name for the extension based on address
    char ext_name[64];
    snprintf(ext_name, sizeof(ext_name), "%s0x%lx", CONFIG_LLE_NAME_PREFIX, address);
    
    int res = llext_load(ldr, ext_name, &ext, &ldr_param);
    if (res != 0) {
        LOG_ERR("Failed to load LLEXT module at 0x%lx, return code %d", address, res);
        return res;
    }
    
    // Store the LLEXT handle in the resource map
    resource_map_set_llext_handle(address, ext);
    
    LOG_INF("Successfully loaded LLEXT module '%s' at 0x%lx", ext_name, address);
    return 0;
}

// Get entry point from loaded LLEXT module
static void* lle_get_entry_point(uintptr_t address)
{
    LOG_DBG("Getting entry point for LLEXT module at 0x%lx", address);
    
    // Retrieve stored ext pointer from resource map
    struct llext *ext = resource_map_get_llext_handle(address);
    if (!ext) {
        LOG_ERR("No LLEXT handle found for address 0x%lx", address);
        return NULL;
    }
    
    // Use configurable entry point symbol
    const char* entry_symbol = CONFIG_LLE_ENTRY_POINT_SYMBOL;
    
    void (*entry_fn)() = llext_find_sym(&ext->exp_tab, entry_symbol);
    if (!entry_fn) {
        LOG_ERR("Failed to find symbol '%s' in LLEXT module at 0x%lx", entry_symbol, address);
        return NULL;
    }
    
    LOG_DBG("Found entry point '%s' at %p for module 0x%lx", entry_symbol, entry_fn, address);
    return (void*)entry_fn;
}

int lle_manager_init(void)
{
    LOG_INF("Initializing LLE manager");
    LOG_DBG("Using entry point symbol: '%s'", CONFIG_LLE_ENTRY_POINT_SYMBOL);
    LOG_DBG("Using name prefix: '%s'", CONFIG_LLE_NAME_PREFIX);
    // LLEXT subsystem should be initialized by Zephyr kernel
    LOG_DBG("LLE manager initialization complete");
    return 0;
}

void* lle_manager_get_entry_point(uintptr_t address)
{
    LOG_DBG("Requested entry point for module at 0x%lx", address);
    
    // Look up the resource in the map
    resource_entry_t* entry = resource_map_find(address);
    if (!entry) {
        LOG_WRN("Address 0x%lx not found in resource map", address);
        return NULL;
    }

    LOG_DBG("Found resource entry: size=%zu, is_lle=%s, is_loaded=%s", 
            entry->size, 
            entry->is_lle_module ? "true" : "false",
            entry->is_loaded ? "true" : "false");

    // Check if it's marked as an LLE module
    if (entry->is_lle_module) {
        // It should already be loaded if it's an LLE
        if (!entry->is_loaded) {
            LOG_WRN("LLE module at 0x%lx is marked as LLE but not loaded - attempting to load", address);
            
            // Attempt to load it using the size from the record
            if (lle_load_module(address, entry->size) != 0) {
                LOG_ERR("Failed to load LLE module at 0x%lx", address);
                return NULL;
            }
            
            // Mark as loaded if successful
            resource_map_set_loaded_status(address, true);
            LOG_DBG("Successfully loaded previously unmarked LLE module at 0x%lx", address);
        } else {
            LOG_DBG("LLE module at 0x%lx already loaded", address);
        }
    } else {
        LOG_DBG("Address 0x%lx not marked as LLE - returning NULL", address);
        return NULL;
    }

    // Get the entry point function pointer
    void* entry_point = lle_get_entry_point(address);
    if (!entry_point) {
        LOG_ERR("Failed to get entry point for LLE module at 0x%lx", address);
        return NULL;
    }

    LOG_INF("Successfully obtained entry point %p for LLE module at 0x%lx", entry_point, address);
    return entry_point;
}

bool lle_manager_unload_if_loaded(uintptr_t address)
{
    LOG_DBG("Checking if address 0x%lx has loaded LLE module", address);
    
    resource_entry_t* entry = resource_map_find(address);
    if (!entry || !entry->is_lle_module || !entry->is_loaded) {
        LOG_DBG("No loaded LLE module found at 0x%lx", address);
        return false;
    }
    
    struct llext *ext = resource_map_get_llext_handle(address);
    if (!ext) {
        LOG_WRN("LLE module at 0x%lx marked as loaded but no handle found", address);
        return false;
    }
    
    LOG_INF("Unloading LLE module at 0x%lx before buffer free", address);
    
    int res = llext_unload(&ext);
    if (res != 0) {
        LOG_ERR("Failed to unload LLE module at 0x%lx, return code %d", address, res);
        return false;
    }
    
    // Clear the handle and update status
    resource_map_set_llext_handle(address, NULL);
    resource_map_set_loaded_status(address, false);
    
    LOG_INF("Successfully unloaded LLE module at 0x%lx", address);
    return true;
}

void lle_manager_cleanup(void)
{
    LOG_INF("Cleaning up LLE manager");
    
    // TODO: Iterate through resource map and unload all LLEXT modules
    // This would require a resource_map_iterate() function or similar
    
    LOG_DBG("LLE manager cleanup complete");
}
