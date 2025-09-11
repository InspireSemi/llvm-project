#include "kernel_processor.h"
#include "lle_manager.h"
#include "resource_map.h"
#include "error_codes.h"
#include "device_specs.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#define DONT_USE_LLEXT 1 // When this variable is defined, LLE related code is excluded and we assume the kernel address is directly executable

/* Stack size (power of 2) */
#define STACK_SIZE 2048
#define PRIORITY 5  /* Preemptible thread */

LOG_MODULE_REGISTER(kernel_processor, LOG_LEVEL_DBG);

// Parameter buffer for future use
static uint8_t parameter_buffer[1024];

// Static thread resources for debugging
static K_THREAD_STACK_ARRAY_DEFINE(thread_stacks, MAX_TOTAL_THREADS, STACK_SIZE);
static struct k_thread thread_data[MAX_TOTAL_THREADS];

static error_code_t check_thread_limits(const launch_cmd_t* launch_cmd, int* total_threads) {
    if (!launch_cmd || !total_threads) {
        return ERR_INVALID_PARAM;
    }

    *total_threads = launch_cmd->grid_x * launch_cmd->grid_y * launch_cmd->grid_z *
                     launch_cmd->block_x * launch_cmd->block_y * launch_cmd->block_z;

    if (*total_threads > MAX_TOTAL_THREADS) {
        LOG_ERR("Total threads %u exceed maximum of %u", *total_threads, MAX_TOTAL_THREADS);
        return ERR_INVALID_PARAM;
    }
    
    return ERR_OK;
}

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

    // Validate thread limits
    int total_threads = 0;
    error_code_t ret = check_thread_limits(launch_cmd, &total_threads);
    if (ret != ERR_OK) {
        return ret;
    }

    #ifndef DONT_USE_LLEXT
        // Mark the memory at kernel_address as an LLE module
        resource_map_set_lle_status(kernel_address, true);
        LOG_DBG("Marked address 0x%lx as LLE module for kernel execution", kernel_address);
        
        // Get the kernel function pointer from LLE manager
        void* kernel_fn = lle_manager_get_entry_point(kernel_address);
        if (!kernel_fn) {
            LOG_ERR("Failed to get kernel function at address 0x%lx", kernel_address);
            return ERR_NOT_FOUND;
        }
        
        LOG_DBG("Found kernel function at %p, executing...", kernel_fn);
    #else
        void* kernel_fn = (void*)kernel_address;
        LOG_DBG("Using direct kernel address %p for execution", kernel_fn);
    #endif

    k_tid_t tids[MAX_TOTAL_THREADS];

    LOG_INF("Creating %d threads for kernel execution", total_threads);

    // Create and start all threads using static stacks
    for (int i = 0; i < total_threads; i++) {
        /* Create thread using pre-allocated static stack */
        tids[i] = k_thread_create(&thread_data[i], 
                                 thread_stacks[i], 
                                 K_THREAD_STACK_SIZEOF(thread_stacks[i]),
                                 (k_thread_entry_t)kernel_fn, 
                                 NULL, NULL, NULL,
                                 PRIORITY, 0, K_NO_WAIT);

        if (!tids[i]) {
            LOG_ERR("Failed to create thread %d", i);
            // Abort any previously created threads
            for (int j = 0; j < i; j++) {
                k_thread_abort(tids[j]);
            }
            return ERR_NO_MEMORY;
        }

        LOG_DBG("Created thread %d with TID %p", i, tids[i]);
    }

    // Wait for all threads to complete
    for (int i = 0; i < total_threads; i++) {
        int join_result = k_thread_join(tids[i], K_SECONDS(10));
        if (join_result != 0) {
            LOG_ERR("Thread %d join failed with result %d", i, join_result);
        } else {
            LOG_DBG("Thread %d completed successfully", i);
        }
    }
    
    LOG_INF("Kernel execution completed successfully");
    return ERR_OK;
}
