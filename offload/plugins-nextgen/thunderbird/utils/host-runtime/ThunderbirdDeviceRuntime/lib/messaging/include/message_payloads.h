/**
 * @file message_payloads.h
 * @brief Message payload structure definitions
 * @details Defines the data structures used in message payloads for different
 *          message types. These structures are transmission-medium independent.
 * @author Michael Brothers (mBrothers@inspiresemi.com)  
 * @version 0.2
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ivshmem_config.h"  // Pull in the base defines
#include "message_slot.h"
#include "error_codes.h"
#include "device_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Compile-time checks for required definitions
#ifndef MESSAGE_SLOT_DATA_SIZE
#error "MESSAGE_SLOT_DATA_SIZE must be defined before including message_payloads.h"
#endif

#ifndef MAX_BATCH_SLOTS
#error "MAX_BATCH_SLOTS must be defined before including message_payloads.h"
#endif

// Ensure all payload structs fit within MESSAGE_SLOT_DATA_SIZE
// Size checks are performed at compile time

typedef struct __attribute__((packed)) {
    uint32_t size;
    uint32_t alignment;
    uint64_t reserved;
} malloc_cmd_t;

typedef struct __attribute__((packed)) {
    error_code_t status;  // Changed from uint32_t to error_code_t
    uint64_t address;
    uint32_t reserved;
} malloc_rsp_t;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint64_t reserved;
} free_cmd_t;

typedef struct __attribute__((packed)) {
    uint64_t kernel_address;
    uint32_t grid_x, grid_y, grid_z;
    uint32_t block_x, block_y, block_z;
    uint64_t args_address;
    uint32_t args_size;      // Size of arguments in bytes
    uint32_t reserved;       // Padding for alignment
} launch_cmd_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint32_t reserved;
} ping_t;

typedef struct __attribute__((packed)) {
    error_code_t status;  // Changed from uint32_t to error_code_t
    uint32_t reserved;
} free_rsp_t;

typedef struct __attribute__((packed)) {
    error_code_t status;  // Changed from uint32_t to error_code_t
    uint32_t reserved;    // Reserved for future use
} launch_rsp_t;

/**
 * @brief Device query command payload
 */
typedef struct __attribute__((packed)) {
    uint32_t query_flags;  // Flags indicating what information to query
    uint32_t reserved;     // Reserved for future use
} query_device_cmd_t;

/**
 * @brief Device query response payload
 */
typedef struct __attribute__((packed)) {
    error_code_t status;   // Status of the query operation
    device_info_t device_info;  // Device information
} query_device_rsp_t;

typedef struct __attribute__((packed)) {
    error_code_t status;          // Status of the query
    uint64_t total_memory;        // Total heap size in bytes
    uint64_t free_memory;         // Available free memory in bytes
    uint32_t reserved;            // Reserved for future use
} memory_status_rsp_t;

// Batch command payloads
typedef struct __attribute__((packed)) {
    uint32_t slot_count;         // Number of slots in this batch (including begin/end)
    uint8_t batch_slots[MAX_BATCH_SLOTS]; // All slot numbers used by this batch
    uint32_t reserved;           // Pad to maintain alignment
} batch_begin_cmd_t;

typedef struct __attribute__((packed)) {
    uint32_t slot_count;         // Number of slots in this batch (must match begin, includes begin/end)
    uint8_t batch_slots[MAX_BATCH_SLOTS]; // same slot numbers as begin command payload's array
    uint32_t reserved;           // Pad to maintain alignment
} batch_end_cmd_t;

/**
 * @brief Batch begin response payload
 */
typedef struct __attribute__((packed)) {
    uint32_t slot_count;                   ///< Number of slots in batch response
    uint8_t batch_slots[MAX_BATCH_SLOTS];  ///< Array of slot indices for batch response
    uint32_t reserved;                     ///< Reserved for future use
} batch_begin_rsp_t;

/**
 * @brief Batch end response payload
 */
typedef struct __attribute__((packed)) {
    uint32_t slot_count;                   ///< Number of slots in batch response
    uint8_t batch_slots[MAX_BATCH_SLOTS];  ///< Array of slot indices for batch response
    uint32_t reserved;                     ///< Reserved for future use
} batch_end_rsp_t;

/**
 * @brief Internal command to invalidate H2D slots
 * Uses standard message layout - sent via router
 */
typedef struct __attribute__((packed)) {
    uint32_t slot_count;                     ///< Number of slots to invalidate
    uint8_t slots[MAX_BATCH_SLOTS];         ///< Array of slot indices to invalidate
    uint32_t reserved;                      ///< Reserved for alignment
} internal_invalidate_slots_cmd_t;

// Compile-time size and offset verification
#ifdef __cplusplus
// C++17 static_assert
static_assert(sizeof(malloc_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "malloc_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(malloc_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "malloc_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(free_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "free_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(free_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "free_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(launch_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "launch_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(launch_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "launch_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(device_info_t) <= MESSAGE_SLOT_DATA_SIZE,
              "device_info_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(query_device_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "query_device_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(query_device_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "query_device_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(ping_t) <= MESSAGE_SLOT_DATA_SIZE,
              "ping_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(memory_status_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "memory_status_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(batch_begin_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "batch_begin_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(batch_end_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "batch_end_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(batch_begin_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "batch_begin_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(batch_end_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
              "batch_end_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
static_assert(sizeof(internal_invalidate_slots_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
              "internal_invalidate_slots_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");

// Offset verification for critical structures
static_assert(offsetof(malloc_cmd_t, size) == 0,
              "malloc_cmd_t.size not at expected offset 0");
static_assert(offsetof(malloc_cmd_t, alignment) == 4,
              "malloc_cmd_t.alignment not at expected offset 4");

static_assert(offsetof(malloc_rsp_t, status) == 0,
              "malloc_rsp_t.status not at expected offset 0");
static_assert(offsetof(malloc_rsp_t, address) == 4,
              "malloc_rsp_t.address not at expected offset 4");

static_assert(offsetof(free_cmd_t, address) == 0,
              "free_cmd_t.address not at expected offset 0");

static_assert(offsetof(free_rsp_t, status) == 0,
              "free_rsp_t.status not at expected offset 0");

static_assert(offsetof(launch_cmd_t, kernel_address) == 0,
              "launch_cmd_t.kernel_address not at expected offset 0");
static_assert(offsetof(launch_cmd_t, grid_x) == 8,
              "launch_cmd_t.grid_x not at expected offset 8");

static_assert(offsetof(launch_rsp_t, status) == 0,
              "launch_rsp_t.status not at expected offset 0");
static_assert(offsetof(launch_rsp_t, reserved) == 4,
              "launch_rsp_t.reserved not at expected offset 4");

static_assert(offsetof(query_device_cmd_t, query_flags) == 0,
              "query_device_cmd_t.query_flags not at expected offset 0");

static_assert(offsetof(query_device_rsp_t, status) == 0,
              "query_device_rsp_t.status not at expected offset 0");
static_assert(offsetof(query_device_rsp_t, device_info) == 4,
              "query_device_rsp_t.device_info not at expected offset 4");

static_assert(offsetof(ping_t, timestamp) == 0,
              "ping_t.timestamp not at expected offset 0");

static_assert(offsetof(memory_status_rsp_t, status) == 0,
              "memory_status_rsp_t.status not at expected offset 0");
static_assert(offsetof(memory_status_rsp_t, total_memory) == 4,
              "memory_status_rsp_t.total_memory not at expected offset 4");
static_assert(offsetof(memory_status_rsp_t, free_memory) == 12,
              "memory_status_rsp_t.free_memory not at expected offset 12");

static_assert(offsetof(batch_begin_cmd_t, slot_count) == 0,
              "batch_begin_cmd_t.slot_count not at expected offset 0");
static_assert(offsetof(batch_begin_cmd_t, batch_slots) == 4,
              "batch_begin_cmd_t.batch_slots not at expected offset 4");
static_assert(offsetof(batch_begin_cmd_t, batch_slots[1]) == offsetof(batch_begin_cmd_t, batch_slots[0]) + sizeof(uint8_t),
              "batch_begin_cmd_t.batch_slots array elements should be packed contiguously");

static_assert(offsetof(batch_end_cmd_t, slot_count) == 0,
              "batch_end_cmd_t.slot_count not at expected offset 0");
static_assert(offsetof(batch_end_cmd_t, batch_slots) == 4,
              "batch_end_cmd_t.batch_slots not at expected offset 4");
static_assert(offsetof(batch_end_cmd_t, batch_slots[1]) == offsetof(batch_end_cmd_t, batch_slots[0]) + sizeof(uint8_t),
              "batch_end_cmd_t.batch_slots array elements should be packed contiguously");

static_assert(offsetof(batch_begin_rsp_t, slot_count) == 0,
              "batch_begin_rsp_t.slot_count not at expected offset 0");
static_assert(offsetof(batch_begin_rsp_t, batch_slots) == 4,
              "batch_begin_rsp_t.batch_slots not at expected offset 4");
static_assert(offsetof(batch_begin_rsp_t, batch_slots[1]) == offsetof(batch_begin_rsp_t, batch_slots[0]) + sizeof(uint8_t),
              "batch_begin_rsp_t.batch_slots array elements should be packed contiguously");

static_assert(offsetof(batch_end_rsp_t, slot_count) == 0,
              "batch_end_rsp_t.slot_count not at expected offset 0");
static_assert(offsetof(batch_end_rsp_t, batch_slots) == 4,
              "batch_end_rsp_t.batch_slots not at expected offset 4");
static_assert(offsetof(batch_end_rsp_t, batch_slots[1]) == offsetof(batch_end_rsp_t, batch_slots[0]) + sizeof(uint8_t),
              "batch_end_rsp_t.batch_slots array elements should be packed contiguously");

static_assert(offsetof(internal_invalidate_slots_cmd_t, slot_count) == 0,
              "internal_invalidate_slots_cmd_t.slot_count not at expected offset 0");
static_assert(offsetof(internal_invalidate_slots_cmd_t, slots) == 4,
              "internal_invalidate_slots_cmd_t.slots not at expected offset 4");

#else
// C11 _Static_assert
_Static_assert(sizeof(malloc_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "malloc_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(malloc_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "malloc_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(free_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "free_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(free_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "free_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(launch_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "launch_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(launch_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "launch_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(device_info_t) <= MESSAGE_SLOT_DATA_SIZE,
               "device_info_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(query_device_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "query_device_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(query_device_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "query_device_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(ping_t) <= MESSAGE_SLOT_DATA_SIZE,
               "ping_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(memory_status_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "memory_status_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(batch_begin_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "batch_begin_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(batch_end_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "batch_end_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(batch_begin_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "batch_begin_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(batch_end_rsp_t) <= MESSAGE_SLOT_DATA_SIZE,
               "batch_end_rsp_t exceeds MESSAGE_SLOT_DATA_SIZE");
_Static_assert(sizeof(internal_invalidate_slots_cmd_t) <= MESSAGE_SLOT_DATA_SIZE,
               "internal_invalidate_slots_cmd_t exceeds MESSAGE_SLOT_DATA_SIZE");

// Offset verification for critical structures
_Static_assert(offsetof(malloc_cmd_t, size) == 0,
               "malloc_cmd_t.size not at expected offset 0");
_Static_assert(offsetof(malloc_cmd_t, alignment) == 4,
               "malloc_cmd_t.alignment not at expected offset 4");

_Static_assert(offsetof(malloc_rsp_t, status) == 0,
               "malloc_rsp_t.status not at expected offset 0");
_Static_assert(offsetof(malloc_rsp_t, address) == 4,
               "malloc_rsp_t.address not at expected offset 4");

_Static_assert(offsetof(free_cmd_t, address) == 0,
               "free_cmd_t.address not at expected offset 0");

_Static_assert(offsetof(free_rsp_t, status) == 0,
               "free_rsp_t.status not at expected offset 0");

_Static_assert(offsetof(launch_cmd_t, kernel_address) == 0,
               "launch_cmd_t.kernel_address not at expected offset 0");
_Static_assert(offsetof(launch_cmd_t, grid_x) == 8,
               "launch_cmd_t.grid_x not at expected offset 8");

_Static_assert(offsetof(launch_rsp_t, status) == 0,
               "launch_rsp_t.status not at expected offset 0");
_Static_assert(offsetof(launch_rsp_t, reserved) == 4,
               "launch_rsp_t.reserved not at expected offset 4");

_Static_assert(offsetof(query_device_cmd_t, query_flags) == 0,
               "query_device_cmd_t.query_flags not at expected offset 0");

_Static_assert(offsetof(query_device_rsp_t, status) == 0,
               "query_device_rsp_t.status not at expected offset 0");
_Static_assert(offsetof(query_device_rsp_t, device_info) == 4,
               "query_device_rsp_t.device_info not at expected offset 4");

_Static_assert(offsetof(ping_t, timestamp) == 0,
               "ping_t.timestamp not at expected offset 0");

_Static_assert(offsetof(memory_status_rsp_t, status) == 0,
               "memory_status_rsp_t.status not at expected offset 0");
_Static_assert(offsetof(memory_status_rsp_t, total_memory) == 4,
               "memory_status_rsp_t.total_memory not at expected offset 4");
_Static_assert(offsetof(memory_status_rsp_t, free_memory) == 12,
               "memory_status_rsp_t.free_memory not at expected offset 12");

_Static_assert(offsetof(batch_begin_cmd_t, slot_count) == 0,
               "batch_begin_cmd_t.slot_count not at expected offset 0");
_Static_assert(offsetof(batch_begin_cmd_t, batch_slots) == 4,
               "batch_begin_cmd_t.batch_slots not at expected offset 4");
_Static_assert(offsetof(batch_begin_cmd_t, batch_slots[1]) == offsetof(batch_begin_cmd_t, batch_slots[0]) + sizeof(uint8_t),
               "batch_begin_cmd_t.batch_slots array elements should be packed contiguously");

_Static_assert(offsetof(batch_end_cmd_t, slot_count) == 0,
               "batch_end_cmd_t.slot_count not at expected offset 0");
_Static_assert(offsetof(batch_end_cmd_t, batch_slots) == 4,
               "batch_end_cmd_t.batch_slots not at expected offset 4");
_Static_assert(offsetof(batch_end_cmd_t, batch_slots[1]) == offsetof(batch_end_cmd_t, batch_slots[0]) + sizeof(uint8_t),
               "batch_end_cmd_t.batch_slots array elements should be packed contiguously");

_Static_assert(offsetof(batch_begin_rsp_t, slot_count) == 0,
               "batch_begin_rsp_t.slot_count not at expected offset 0");
_Static_assert(offsetof(batch_begin_rsp_t, batch_slots) == 4,
               "batch_begin_rsp_t.batch_slots not at expected offset 4");
_Static_assert(offsetof(batch_begin_rsp_t, batch_slots[1]) == offsetof(batch_begin_rsp_t, batch_slots[0]) + sizeof(uint8_t),
               "batch_begin_rsp_t.batch_slots array elements should be packed contiguously");

_Static_assert(offsetof(batch_end_rsp_t, slot_count) == 0,
               "batch_end_rsp_t.slot_count not at expected offset 0");
_Static_assert(offsetof(batch_end_rsp_t, batch_slots) == 4,
               "batch_end_rsp_t.batch_slots not at expected offset 4");
_Static_assert(offsetof(batch_end_rsp_t, batch_slots[1]) == offsetof(batch_end_rsp_t, batch_slots[0]) + sizeof(uint8_t),
               "batch_end_rsp_t.batch_slots array elements should be packed contiguously");

_Static_assert(offsetof(internal_invalidate_slots_cmd_t, slot_count) == 0,
               "internal_invalidate_slots_cmd_t.slot_count not at expected offset 0");
_Static_assert(offsetof(internal_invalidate_slots_cmd_t, slots) == 4,
               "internal_invalidate_slots_cmd_t.slots not at expected offset 4");

#endif

#ifdef __cplusplus
}
#endif