/*
 * SAXPY (Single-precision A*X Plus Y) loadable extension implementation.
 * This extension performs the SAXPY operation: out[i] = a * x[i] + y[i]
 * across multiple threads for parallel execution.
 */

#include "error_codes.h"
#include "api_table.h"
#include "device_specs.h"
#include "ivshmem_config.h" // Needs to be fixed to refer to right type of device interface, this is only for testing QEMU model

#include <zephyr/llext/symbol.h>

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
    
    // Calculate chunk boundaries for this thread
    int n = real_args->n;
    int threads = real_args->num_threads;
    int chunk_size = (n + threads - 1) / threads;
    int start_index = local_thread_id * chunk_size;
    int stop_index = start_index + chunk_size;
    if (stop_index > n) stop_index = n;
    if (start_index >= stop_index) return;
    
    // Perform SAXPY calculation using helper function
    perform_saxpy_chunk(real_args->x, real_args->y, real_args->out, 
                       real_args->a, start_index, stop_index);

    // Completion message
    if (local_thread_id == 0) {
        const char *msg2 = "SAXPY kernel completed";
        api->print_msg(local_thread_id, msg2);
    }

}
EXPORT_SYMBOL(execute);
