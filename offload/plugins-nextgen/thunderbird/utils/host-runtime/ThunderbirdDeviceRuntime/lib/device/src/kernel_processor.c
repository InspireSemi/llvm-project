#include "kernel_processor.h"
#include "kernel_dispatch.h"
#include "lle_manager.h"
#include "resource_map.h"
#include "error_codes.h"
#include "device_specs.h"
#include "api_table.h"
#include "api_functions.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>

#include <stdio.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#define DONT_USE_LLEXT 0 // When this variable is defined, LLE related code is excluded and we assume the kernel address is directly executable

/* Stack size (power of 2) */
#define STACK_SIZE 2048
#define PRIORITY 5  /* Preemptible thread */

LOG_MODULE_REGISTER(kernel_processor, LOG_LEVEL_DBG);

typedef struct {
    void (*kernel_fn)(void*);  // Kernel function pointer (will be cast based on arg count)
    void *args_address;        // Pointer to argument data
    size_t args_size;          // Size of arguments in bytes
} kernel_launch_params_t;

static void kernel_wrapper(void *params, void *unused1, void *unused2) {
    kernel_launch_params_t *launch_params = (kernel_launch_params_t*)params;
    
    // The kernel function signature is actually variable based on argument count
    // OpenMP kernels receive arguments as: kernel_fn(KernelEnvironmentTy*, arg1, arg2, ...)
    // We treat all args as void* and use the dispatcher to handle variable signatures
    
    void **ptr_array = (void**)launch_params->args_address;
    
    // Calculate number of pointer-sized arguments
    // Note: args_size is the total size of the argument blob
    size_t num_args = launch_params->args_size / sizeof(void*);
    
    // Dispatch to the appropriate kernel signature using jump table
    error_code_t err = dispatch_kernel(launch_params->kernel_fn, ptr_array, num_args);
    if (err != ERR_OK) {
        LOG_ERR("Kernel dispatch failed with error %d", err);
        return;
    }
    
    LOG_INF("Kernel returned");
}

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
    
    // Extract kernel and args addresses
    uintptr_t kernel_address = (uintptr_t)launch_cmd->kernel_address;
    uintptr_t args_address = (uintptr_t)launch_cmd->args_address;

    // Populate API table with all available functions
    api_table_t api_table;
    api_populate_table(&api_table);
   
    LOG_INF("Executing kernel at address 0x%lx", kernel_address);
    LOG_DBG("Grid dimensions: (%u, %u, %u)", 
           launch_cmd->grid_x, launch_cmd->grid_y, launch_cmd->grid_z);
    LOG_DBG("Block dimensions: (%u, %u, %u)", 
           launch_cmd->block_x, launch_cmd->block_y, launch_cmd->block_z);
    LOG_DBG("Arguments address: 0x%llx", launch_cmd->args_address);

    // Validate thread limits
    int total_threads = 0;
    error_code_t ret = check_thread_limits(launch_cmd, &total_threads);
    if (ret != ERR_OK) {
        return ret;
    }

  #if !DONT_USE_LLEXT
    // Find which module contains this kernel
    resource_entry_t* entry = resource_map_find(kernel_address);  // Use find(), not find_containing()
    if (!entry) {
        LOG_ERR("No resource found for kernel address 0x%lx", kernel_address);
        return ERR_NOT_FOUND;
    }

    // Mark it as an LLE module
    resource_map_set_lle_status(kernel_address, true);
    
    // Load the LLEXT module if not already loaded
    if (!entry->is_loaded) {
        int load_res = lle_manager_load_module(entry->address, entry->size);
        if (load_res != 0) {
            LOG_ERR("Failed to load LLEXT module at 0x%lx, code %d", entry->address, load_res);
            return ERR_UNKNOWN;
        }
    }
    
    // Get the kernel function pointer - pass the EXACT base address
    void* kernel_fn = lle_manager_get_entry_point(entry->address);
    if (!kernel_fn) {
        LOG_ERR("Failed to get kernel function at address 0x%lx", entry->address);
        return ERR_NOT_FOUND;
    }
#else
    void* kernel_fn = (void*)kernel_address;
#endif

    k_tid_t tids[MAX_TOTAL_THREADS];
    static kernel_launch_params_t launch_params[MAX_TOTAL_THREADS];

    LOG_INF("Creating %d threads for kernel execution", total_threads);
    int irregular_result = 0;


    for (int i = 0; i < total_threads; i++) {
        launch_params[i].kernel_fn = kernel_fn;
        launch_params[i].args_address = (void*)args_address;
        launch_params[i].args_size = (size_t)launch_cmd->args_size;  // Pass the args size
        
        tids[i] = k_thread_create(&thread_data[i],
                                thread_stacks[i],
                                K_THREAD_STACK_SIZEOF(thread_stacks[i]),
                                (k_thread_entry_t)kernel_wrapper,
                                &launch_params[i], NULL, NULL,
                                PRIORITY, 0, K_NO_WAIT);
        
        if (!tids[i]) {
            LOG_ERR("Failed to create thread %d", i);
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
            irregular_result = join_result;
            LOG_ERR("Thread %d join failed with result %d", i, join_result);
        } else {
            LOG_DBG("Thread %d completed successfully", i);
        }
    }
    
    LOG_INF("Kernel execution completed successfully");

    return irregular_result == 0 ? ERR_OK : ERR_UNKNOWN;
}
