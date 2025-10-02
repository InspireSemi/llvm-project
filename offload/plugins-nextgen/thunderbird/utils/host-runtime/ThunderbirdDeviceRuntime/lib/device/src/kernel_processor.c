#include "kernel_processor.h"
#include "lle_manager.h"
#include "resource_map.h"
#include "error_codes.h"
#include "device_specs.h"
#include "api_table.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
//#define DONT_USE_LLEXT 1 // When this variable is defined, LLEXT related code is excluded and we assume the kernel address is directly executable

/* Stack size (power of 2) */
#define STACK_SIZE 2048
#define PRIORITY 5  /* Preemptible thread */

LOG_MODULE_REGISTER(kernel_processor, LOG_LEVEL_DBG);

// Static thread resources for debugging
static K_THREAD_STACK_ARRAY_DEFINE(thread_stacks, MAX_TOTAL_THREADS, STACK_SIZE);
static struct k_thread thread_data[MAX_TOTAL_THREADS];

// True circular buffer for worker thread messages - fixed size, overwrites when full
#define MESSAGE_BUFFER_SIZE 8
static char worker_message_buffer[MESSAGE_BUFFER_SIZE][128];
static volatile int message_write_index = 0;
static volatile int message_count = 0;
static K_MUTEX_DEFINE(message_buffer_mutex);

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

// True circular buffer - overwrites oldest messages when full
static void api_store_message(int ltid, const char* str) {
    k_mutex_lock(&message_buffer_mutex, K_FOREVER);
    
    if (str != NULL) {
        int write_pos = message_write_index;
        
        // Safe string copy with bounds, including ltid in the message
        snprintf(worker_message_buffer[write_pos], 127, "[Thread %d]: %s", ltid, str);
        worker_message_buffer[write_pos][127] = '\0';  // Force null termination
        
        // Always advance write position (circular)
        message_write_index = (message_write_index + 1) % MESSAGE_BUFFER_SIZE;
        
        // Track count up to buffer size, then it stays at max
        if (message_count < MESSAGE_BUFFER_SIZE) {
            message_count++;
        }
        // If count == MESSAGE_BUFFER_SIZE, we're overwriting the oldest message
    }
    
    k_mutex_unlock(&message_buffer_mutex);
}

// Read from true circular buffer
static void print_and_clear_worker_messages(void) {
    k_mutex_lock(&message_buffer_mutex, K_FOREVER);
    
    int count = message_count;
    LOG_INF("Worker thread messages (%d total, buffer size %d):", count, MESSAGE_BUFFER_SIZE);
    
    if (count > 0) {
        // Calculate start position for reading
        // If buffer is full, start from oldest message (current write position)
        // If buffer is partial, start from beginning (position 0)
        int read_start;
        if (count == MESSAGE_BUFFER_SIZE) {
            // Buffer is full - oldest message is at current write position
            read_start = message_write_index;
        } else {
            // Buffer is partial - messages start at position 0
            read_start = 0;
        }
        
        // Print messages in chronological order (oldest first)
        for (int i = 0; i < count; i++) {
            int read_pos = (read_start + i) % MESSAGE_BUFFER_SIZE;
            LOG_INF("  Message %d: %s", i + 1, worker_message_buffer[read_pos]);
        }
    }
    
    // Clear buffer for next run
    message_count = 0;
    message_write_index = 0;
    
    k_mutex_unlock(&message_buffer_mutex);
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

    // Populate API table
    api_table_t api_table;
    api_table._API_TABLE_VERSION = API_TABLE_VERSION;
    api_table._IVSHMEM_ABI_VERSION = IVSHMEM_ABI_VERSION;
    api_table.print_msg = api_store_message;  // Non-variadic function
   
    LOG_INF("Executing kernel at address 0x%lx", kernel_address);
    LOG_DBG("Grid dimensions: (%u, %u, %u)", 
           launch_cmd->grid_x, launch_cmd->grid_y, launch_cmd->grid_z);
    LOG_DBG("Block dimensions: (%u, %u, %u)", 
           launch_cmd->block_x, launch_cmd->block_y, launch_cmd->block_z);
    LOG_DBG("Arguments address: 0x%llx", launch_cmd->args_address);

    LOG_DBG("multi_var.out args (exactly 3 args) %u %u %u", *((uint32_t *) args_address), *(((uint32_t *) args_address)+1),  *(((uint32_t *) args_address)+2));

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
    int ltids[MAX_TOTAL_THREADS]; // Local thread IDs

    LOG_INF("Creating %d threads for kernel execution", total_threads);
    int irregular_result = 0;
    // Create and start all threads using static stacks
    for (int i = 0; i < total_threads; i++) {
        /* Create thread using pre-allocated static stack */
        ltids[i] = i; // Local thread ID
        tids[i] = k_thread_create(&thread_data[i], 
                                 thread_stacks[i], 
                                 K_THREAD_STACK_SIZEOF(thread_stacks[i]),
                                 (k_thread_entry_t)kernel_fn, 
                                 (void*)(&args_address), (void*)(&api_table), (void*)(&ltids[i]),
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
            irregular_result = join_result;
            LOG_ERR("Thread %d join failed with result %d", i, join_result);
        } else {
            LOG_DBG("Thread %d completed successfully", i);
        }
    }
    
    LOG_INF("Kernel execution completed successfully");

    // Print and clear worker messages using circular buffer
    print_and_clear_worker_messages();

    return irregular_result == 0 ? ERR_OK : ERR_UNKNOWN;
}
