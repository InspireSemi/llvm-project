#include "kernel_dispatch.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(kernel_processor);

// Type definitions for each argument count
typedef void (*kernel_fn_0_t)(void);
typedef void (*kernel_fn_1_t)(void*);
typedef void (*kernel_fn_2_t)(void*, void*);
typedef void (*kernel_fn_3_t)(void*, void*, void*);
typedef void (*kernel_fn_4_t)(void*, void*, void*, void*);
typedef void (*kernel_fn_5_t)(void*, void*, void*, void*, void*);
typedef void (*kernel_fn_6_t)(void*, void*, void*, void*, void*, void*);
typedef void (*kernel_fn_7_t)(void*, void*, void*, void*, void*, void*, void*);
typedef void (*kernel_fn_8_t)(void*, void*, void*, void*, void*, void*, void*, void*);

// Individual caller functions - each casts to appropriate signature and calls
static void call_kernel_0(void *fn, void **args) {
    ((kernel_fn_0_t)fn)();
}

static void call_kernel_1(void *fn, void **args) {
    ((kernel_fn_1_t)fn)(args[0]);
}

static void call_kernel_2(void *fn, void **args) {
    ((kernel_fn_2_t)fn)(args[0], args[1]);
}

static void call_kernel_3(void *fn, void **args) {
    ((kernel_fn_3_t)fn)(args[0], args[1], args[2]);
}

static void call_kernel_4(void *fn, void **args) {
    ((kernel_fn_4_t)fn)(args[0], args[1], args[2], args[3]);
}

static void call_kernel_5(void *fn, void **args) {
    ((kernel_fn_5_t)fn)(args[0], args[1], args[2], args[3], args[4]);
}

static void call_kernel_6(void *fn, void **args) {
    ((kernel_fn_6_t)fn)(args[0], args[1], args[2], args[3], args[4], args[5]);
}

static void call_kernel_7(void *fn, void **args) {
    ((kernel_fn_7_t)fn)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
}

static void call_kernel_8(void *fn, void **args) {
    ((kernel_fn_8_t)fn)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
}

// Jump table - index by argument count to get the appropriate caller function
static void (*kernel_callers[])(void*, void**) = {
    call_kernel_0,
    call_kernel_1,
    call_kernel_2,
    call_kernel_3,
    call_kernel_4,
    call_kernel_5,
    call_kernel_6,
    call_kernel_7,
    call_kernel_8
};

#define MAX_SUPPORTED_ARGS (sizeof(kernel_callers) / sizeof(kernel_callers[0]))

error_code_t dispatch_kernel(void (*kernel_fn)(void*), void **args, size_t num_args) {
    if (num_args >= MAX_SUPPORTED_ARGS) {
        LOG_ERR("Unsupported number of kernel arguments: %zu (max: %zu)", 
                num_args, MAX_SUPPORTED_ARGS - 1);
        return ERR_INVALID_PARAM;
    }
    
    // Use jump table to call the appropriate wrapper
    kernel_callers[num_args](kernel_fn, args);
    return ERR_OK;
}

size_t dispatch_kernel_max_args(void) {
    return MAX_SUPPORTED_ARGS - 1;
}
