#include "memory_command_handler.h"
#include "device_memory.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(memory_command_handler, LOG_LEVEL_DBG);

error_code_t handle_malloc_command(const malloc_cmd_t* cmd, malloc_result_t* result)
{
    if (!cmd || !result) {
        return ERR_INVALID_PARAM;
    }

    LOG_DBG("Handling malloc command: size=%u, alignment=%u", cmd->size, cmd->alignment);

    // Validate parameters
    if (cmd->size == 0) {
        LOG_ERR("Invalid malloc size: 0");
        result->status = ERR_INVALID_PARAM;
        result->address = 0;
        return ERR_INVALID_PARAM;
    }

    // Log alignment parameter (ignored)
    if (cmd->alignment != 0 && cmd->alignment != 1) {
        LOG_WRN("Alignment parameter (%u) ignored - using default alignment", cmd->alignment);
    }

    // Execute memory allocation using shared memory
    void *allocated_ptr = device_shared_malloc(cmd->size);
    
    error_code_t alloc_result;
    if (allocated_ptr != NULL) {
        result->address = (uint64_t)(uintptr_t)allocated_ptr;
        alloc_result = ERR_OK;
    } else {
        result->address = 0;
        alloc_result = ERR_NO_MEMORY;  // Assuming this error code exists
    }
    
    result->status = alloc_result;
    
    if (alloc_result == ERR_OK) {
        LOG_DBG("Malloc succeeded: allocated %u bytes at 0x%llx", cmd->size, result->address);
    } else {
        LOG_ERR("Malloc failed: %d", alloc_result);
        result->address = 0;
    }

    return alloc_result;
}

error_code_t handle_free_command(const free_cmd_t* cmd)
{
    if (!cmd) {
        return ERR_INVALID_PARAM;
    }

    LOG_DBG("Handling free command: address=0x%llx", cmd->address);

    // Validate address
    if (cmd->address == 0) {
        LOG_WRN("Attempting to free NULL address");
        return ERR_INVALID_PARAM;
    }

    // Execute memory deallocation using shared memory
    void *ptr_to_free = (void *)(uintptr_t)cmd->address;
    device_shared_free(ptr_to_free);
    
    // device_shared_free doesn't return an error code, assume success
    error_code_t free_result = ERR_OK;
    
    if (free_result == ERR_OK) {
        LOG_DBG("Free succeeded: deallocated address 0x%llx", cmd->address);
    } else {
        LOG_ERR("Free failed: %d", free_result);
    }

    return free_result;
}
