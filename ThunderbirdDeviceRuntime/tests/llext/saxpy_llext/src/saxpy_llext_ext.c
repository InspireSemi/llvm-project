/*
 * SAXPY (Single-precision A*X Plus Y) loadable extension implementation.
 * This extension performs the SAXPY operation: out[i] = a * x[i] + y[i]
 * across multiple threads for parallel execution.
 */

#include "error_codes.h"
#include "api_table.h"
#include "device_specs.h"
#include "ivshmem_config.h" // Needs to be fixed to refer to right type of device interface, this is only for testing QEMU model
#include "cache_map.h"

#include <zephyr/llext/symbol.h>
#include <stdio.h>

// Packed structure for SAXPY kernel arguments
typedef struct __attribute__((packed)) {
    const float * const x;
    const float * const y;
    float * const out;
    float a;
    int n;
    int num_threads;
} args_t;

// Static inline helper function for SAXPY calculation. 
// Needs to be inlined or when the flat binary is made it will not link without intervention we aren't doing yet.
static inline __attribute__((always_inline)) void perform_saxpy_chunk(const float * restrict const x, // Restrict is only valid if we are sure pointers do not alias.
                                     const float * restrict const y,
                                     float * restrict const out,
                                     const float a,
                                     const int start_index, 
                                     const int stop_index) {
    // Perform SAXPY operation: out[i] = a * x[i] + y[i]
    for (int i = start_index; i < stop_index; i++) {
        out[i] = a * x[i] + y[i];
    }
}

void execute(void* z_args, void* z_api_table, void* z_ltid){
    // Minimal validation
    if (!z_args || !z_api_table || !z_ltid) return;

    // Simple casts
    int local_thread_id = *(int*)z_ltid;
    args_t* real_args = (args_t*)(*(uintptr_t*)z_args);
    api_table_t* api = (api_table_t*)z_api_table;

    // Debug message from kernel
    if (local_thread_id == 0) {
        const char *msg1 = "SAXPY kernel executing";
        api->print_msg(local_thread_id, msg1);
    }

    // Basic validation
    if (!real_args || !real_args->x || !real_args->y || !real_args->out) return;
    if (real_args->n <= 0 || real_args->num_threads <= 0) return;
    if (local_thread_id >= real_args->num_threads) return;
    
    // Get cache pairs for x, y, and out
    cache_pair_t *pair_x = cache_map_find_by_shared((uintptr_t)real_args->x);
    cache_pair_t *pair_y = cache_map_find_by_shared((uintptr_t)real_args->y);
    cache_pair_t *pair_out = cache_map_find_by_shared((uintptr_t)real_args->out);
    if (!pair_x || !pair_y || !pair_out) {
        if (local_thread_id == 0) {
            const char *msg_err = "Failed to find cache pairs for x, y, or out";
            api->print_msg(local_thread_id, msg_err);
        }
        return;
    }
    
    // Get exclusive addresses from the pairs
    uintptr_t excl_x = pair_x->exclusive_address;
    uintptr_t excl_y = pair_y->exclusive_address;
    uintptr_t excl_out = pair_out->exclusive_address;
    
    // Cast to float pointers for processing
    const float *excl_x_ptr = (const float *)excl_x;
    const float *excl_y_ptr = (const float *)excl_y;
    float *excl_out_ptr = (float *)excl_out;
    
    // Calculate chunk boundaries for this thread
    int n = real_args->n;
    int threads = real_args->num_threads;
    int chunk_size = (n + threads - 1) / threads;
    int start_index = local_thread_id * chunk_size;
    int stop_index = start_index + chunk_size;
    if (stop_index > n) stop_index = n;
    if (start_index >= stop_index) return;
    
    // Compute sync parameters for this thread's chunk
    size_t offset = start_index * sizeof(float);
    size_t num_elements = stop_index - start_index;
    
    // Sync this thread's chunk of input data from shared to exclusive for x and y
    int sync_ret_x = cache_map_sync_shared_to_exclusive(pair_x, offset, sizeof(float), num_elements);
    int sync_ret_y = cache_map_sync_shared_to_exclusive(pair_y, offset, sizeof(float), num_elements);
    if (sync_ret_x != 0 || sync_ret_y != 0) {
        const char *msg_err = "Failed to sync shared to exclusive for x or y chunk";
        api->print_msg(local_thread_id, msg_err);
        return;
    }
    
    // Add comment from each thread about what they synced (x and y)
    {
        char msg_sync[128];
        api->snprintf(msg_sync, sizeof(msg_sync), "Thread %d synced x and y chunks (start %d, stop %d) from shared to exclusive", local_thread_id, start_index, stop_index);
        api->print_msg(local_thread_id, msg_sync);
    }
    
    // Perform SAXPY calculation on exclusive buffers for this thread's chunk
    perform_saxpy_chunk(excl_x_ptr, excl_y_ptr, excl_out_ptr, 
                       real_args->a, start_index, stop_index);
    
    // Mark out's exclusive buffer as dirty after modification (per thread, but atomic)
    cache_map_set_status(pair_out, true, true);

    
    // Sync this thread's output chunk from exclusive to shared
    int sync_ret_out = cache_map_sync_exclusive_to_shared(pair_out, offset, sizeof(float), num_elements);
    if (sync_ret_out != 0) {
        const char *msg_err = "Failed to sync exclusive to shared for out chunk";
        api->print_msg(local_thread_id, msg_err);
        return;
    }
    
    // Add comment from each thread about what they synced (out)
    {
        char msg_sync_out[128];
        api->snprintf(msg_sync_out, sizeof(msg_sync_out), "Thread %d synced out chunk (start %d, stop %d) from exclusive to shared", local_thread_id, start_index, stop_index);
        api->print_msg(local_thread_id, msg_sync_out);
    }

    // Completion message
    if (local_thread_id == 0) {
        const char *msg2 = "SAXPY kernel completed";
        api->print_msg(local_thread_id, msg2);
    }

}
EXPORT_SYMBOL(execute);
