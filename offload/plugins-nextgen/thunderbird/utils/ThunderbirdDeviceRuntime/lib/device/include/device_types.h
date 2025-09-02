#pragma once

#include <stdint.h>
#include <stddef.h>
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device information structure containing device capabilities and memory info
 */
typedef struct __attribute__((packed)) {
    uint32_t compute_capability_major;
    uint32_t compute_capability_minor;
    uint32_t max_threads_per_block;
    uint32_t max_blocks_per_multiprocessor;
    uint64_t total_global_memory;
    uint32_t shared_memory_per_block;
    uint32_t registers_per_block;
    uint32_t warp_size;
} device_info_t;

/**
 * @brief Result structure for device query command execution
 */
typedef struct __attribute__((packed)) {
    error_code_t status;
    device_info_t device_info;
} query_result_t;

/**
 * @brief Result structure for launch command execution
 */
typedef struct __attribute__((packed)) {
    error_code_t status;
    uint32_t launch_id;  // Optional: ID for tracking the launched kernel
} launch_result_t;

#ifdef __cplusplus
}

static_assert(sizeof(device_info_t) == 36, "device_info_t size mismatch");
static_assert(offsetof(device_info_t, compute_capability_major) == 0,
              "device_info_t.compute_capability_major not at expected offset 0");
static_assert(offsetof(device_info_t, total_global_memory) == 16,
              "device_info_t.total_global_memory not at expected offset 16");

static_assert(sizeof(query_result_t) == 40, "query_result_t size mismatch");
static_assert(offsetof(query_result_t, status) == 0,
              "query_result_t.status not at expected offset 0");
static_assert(offsetof(query_result_t, device_info) == 4,
              "query_result_t.device_info not at expected offset 4");

static_assert(sizeof(launch_result_t) == 8, "launch_result_t size mismatch");
static_assert(offsetof(launch_result_t, status) == 0,
              "launch_result_t.status not at expected offset 0");
static_assert(offsetof(launch_result_t, launch_id) == 4,
              "launch_result_t.launch_id not at expected offset 4");
#else
_Static_assert(sizeof(device_info_t) == 36, "device_info_t size mismatch");
_Static_assert(offsetof(device_info_t, compute_capability_major) == 0,
               "device_info_t.compute_capability_major not at expected offset 0");
_Static_assert(offsetof(device_info_t, total_global_memory) == 16,
               "device_info_t.total_global_memory not at expected offset 16");

_Static_assert(sizeof(query_result_t) == 40, "query_result_t size mismatch");
_Static_assert(offsetof(query_result_t, status) == 0,
               "query_result_t.status not at expected offset 0");
_Static_assert(offsetof(query_result_t, device_info) == 4,
               "query_result_t.device_info not at expected offset 4");

_Static_assert(sizeof(launch_result_t) == 8, "launch_result_t size mismatch");
_Static_assert(offsetof(launch_result_t, status) == 0,
               "launch_result_t.status not at expected offset 0");
_Static_assert(offsetof(launch_result_t, launch_id) == 4,
               "launch_result_t.launch_id not at expected offset 4");
#endif
