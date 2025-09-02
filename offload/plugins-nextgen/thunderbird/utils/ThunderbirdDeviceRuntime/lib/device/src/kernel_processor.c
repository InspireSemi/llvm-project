#include "kernel_processor.h"
#include "lle_manager.h"
#include "resource_map.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kernel_processor, LOG_LEVEL_DBG);

// Parameter buffer for future use
static uint8_t parameter_buffer[1024];

// Main processing function
int kernel_processor_execute(const launch_cmd_t* launch_cmd) {
    if (!launch_cmd) {
        LOG_ERR("Invalid launch_cmd parameter");
        return ERR_INVALID_PARAM;
    }
    
    uintptr_t kernel_address = (uintptr_t)launch_cmd->kernel_address;
    
    LOG_INF("Executing kernel at address 0x%lx", kernel_address);
    LOG_DBG("Grid dimensions: (%u, %u, %u)", 
           launch_cmd->grid_x, launch_cmd->grid_y, launch_cmd->grid_z);
    LOG_DBG("Block dimensions: (%u, %u, %u)", 
           launch_cmd->block_x, launch_cmd->block_y, launch_cmd->block_z);
    LOG_DBG("Shared memory size: %u bytes", launch_cmd->shared_mem_size);
    
    // Just-in-time: Mark the memory at kernel_address as an LLE module
    // This allows the system to be more user-friendly - any memory can potentially contain an LLEXT
    resource_map_set_lle_status(kernel_address, true);
    LOG_DBG("Marked address 0x%lx as LLE module for kernel execution", kernel_address);
    
    // Get the kernel function pointer from LLE manager
    void* kernel_fn = lle_manager_get_entry_point(kernel_address);
    if (!kernel_fn) {
        LOG_ERR("Failed to get kernel function at address 0x%lx", kernel_address);
        return ERR_NOT_FOUND;
    }
    
    LOG_DBG("Found kernel function at %p, executing...", kernel_fn);
    
    // Execute the kernel function - cast to correct void signature
    void (*fn)(void) = (void (*)(void))kernel_fn;
    fn();
    
    LOG_INF("Kernel execution completed successfully");
    return ERR_OK;
}
