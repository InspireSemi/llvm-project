/*
 * SAXPY (Single-precision A*X Plus Y) loadable extension implementation.
 * This extension performs the SAXPY operation: out[i] = a * x[i] + y[i]
 * across multiple threads for parallel execution.
 */

#include "error_codes.h"
#include "api_table.h"
#include "device_specs.h"
#include "ivshmem_config.h" // Needs to be fixed to refer to right type of device interface, this is only for testing QEMU model

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

	// Debug message from kernel - build string manually at runtime
    if (local_thread_id == 0) {
        // Build string character by character to avoid any section dependencies
        char msg1[22];  // "SAXPY kernel executing" + null terminator
        msg1[0] = 'S'; msg1[1] = 'A'; msg1[2] = 'X'; msg1[3] = 'P'; msg1[4] = 'Y';
        msg1[5] = ' '; msg1[6] = 'k'; msg1[7] = 'e'; msg1[8] = 'r'; msg1[9] = 'n';
        msg1[10] = 'e'; msg1[11] = 'l'; msg1[12] = ' '; msg1[13] = 'e'; msg1[14] = 'x';
        msg1[15] = 'e'; msg1[16] = 'c'; msg1[17] = 'u'; msg1[18] = 't'; msg1[19] = 'i';
        msg1[20] = 'n'; msg1[21] = 'g'; msg1[22] = '\0';
        
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
        // Build completion string
        char msg2[21];  // "SAXPY kernel completed" + null terminator  
        msg2[0] = 'S'; msg2[1] = 'A'; msg2[2] = 'X'; msg2[3] = 'P'; msg2[4] = 'Y';
        msg2[5] = ' '; msg2[6] = 'k'; msg2[7] = 'e'; msg2[8] = 'r'; msg2[9] = 'n';
        msg2[10] = 'e'; msg2[11] = 'l'; msg2[12] = ' '; msg2[13] = 'c'; msg2[14] = 'o';
        msg2[15] = 'm'; msg2[16] = 'p'; msg2[17] = 'l'; msg2[18] = 'e'; msg2[19] = 't';
        msg2[20] = 'e'; msg2[21] = 'd'; msg2[22] = '\0';
        
        api->print_msg(local_thread_id, msg2);
    }

}
