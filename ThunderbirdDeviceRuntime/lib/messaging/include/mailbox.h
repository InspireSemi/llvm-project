/**
 * @file mailbox.h
 * @brief Mailbox data structure definition
 * @details Defines the mailbox structure used for inter-VM communication
 *          over IVSHMEM. Uses a slots_occupied array to track slot usage
 *          instead of scalar head/tail/count members.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-08-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ivshmem_config.h"  // Pull in the base defines
#include "message_slot.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAILBOX_SLOT_COUNT
#error "MAILBOX_SLOT_COUNT must be defined before including mailbox.h"
#endif

#ifndef MAILBOX_MAX_SIZE_BYTES
#error "MAILBOX_MAX_SIZE_BYTES must be defined before including mailbox.h"
#endif

typedef struct __attribute__((packed, aligned(8))) {
    uint8_t slots_occupied[MAILBOX_SLOT_COUNT];
    message_slot_t slots[MAILBOX_SLOT_COUNT];
} mailbox_t;

// Helper macros for stringification (to show values in error messages)
#define MY_STRINGIFY(x) #x
#define TOSTRING(x) MY_STRINGIFY(x)

// Static assertions for size and alignment verification
#ifdef __cplusplus
// C++ static assertions with enhanced error messages
static_assert(sizeof(mailbox_t) <= MAILBOX_MAX_SIZE_BYTES, 
              "mailbox_t size exceeds MAILBOX_MAX_SIZE_BYTES limit. "
              "Check that MESSAGE_SLOT_MAX_SIZE_BYTES (" TOSTRING(MESSAGE_SLOT_MAX_SIZE_BYTES) "), "
              "MAILBOX_SLOT_COUNT (" TOSTRING(MAILBOX_SLOT_COUNT) "), "
              "and slots_occupied array size result in MAILBOX_MAX_SIZE_BYTES (" TOSTRING(MAILBOX_MAX_SIZE_BYTES) ") "
              "being large enough for the actual mailbox_t structure");

// Updated calculation with slots_occupied array
static_assert(MAILBOX_SLOT_COUNT + (MAILBOX_SLOT_COUNT * sizeof(message_slot_t)) <= MAILBOX_MAX_SIZE_BYTES,
              "Calculated mailbox size (MAILBOX_SLOT_COUNT bytes for slots_occupied + " 
              TOSTRING(MAILBOX_SLOT_COUNT) " slots * sizeof(message_slot_t)) "
              "exceeds MAILBOX_MAX_SIZE_BYTES (" TOSTRING(MAILBOX_MAX_SIZE_BYTES) ")");

static_assert(offsetof(mailbox_t, slots_occupied) == 0, 
              "slots_occupied member offset should be 0");
static_assert(offsetof(mailbox_t, slots) == MAILBOX_SLOT_COUNT, 
              "slots member offset should be MAILBOX_SLOT_COUNT");
static_assert(offsetof(mailbox_t, slots[1]) == offsetof(mailbox_t, slots[0]) + sizeof(message_slot_t),
              "slots array elements should be packed contiguously");
static_assert(sizeof(mailbox_t) % 8 == 0, 
              "mailbox_t size should be 8-byte aligned");
#else
// C11 static assertions with enhanced error messages
_Static_assert(sizeof(mailbox_t) <= MAILBOX_MAX_SIZE_BYTES, 
               "mailbox_t size exceeds MAILBOX_MAX_SIZE_BYTES limit. "
               "Check that MESSAGE_SLOT_MAX_SIZE_BYTES (" TOSTRING(MESSAGE_SLOT_MAX_SIZE_BYTES) "), "
               "MAILBOX_SLOT_COUNT (" TOSTRING(MAILBOX_SLOT_COUNT) "), "
               "and slots_occupied array size result in MAILBOX_MAX_SIZE_BYTES (" TOSTRING(MAILBOX_MAX_SIZE_BYTES) ") "
               "being large enough for the actual mailbox_t structure");

// Updated calculation with slots_occupied array
_Static_assert(MAILBOX_SLOT_COUNT + (MAILBOX_SLOT_COUNT * sizeof(message_slot_t)) <= MAILBOX_MAX_SIZE_BYTES,
               "Calculated mailbox size (MAILBOX_SLOT_COUNT bytes for slots_occupied + " 
               TOSTRING(MAILBOX_SLOT_COUNT) " slots * sizeof(message_slot_t)) "
               "exceeds MAILBOX_MAX_SIZE_BYTES (" TOSTRING(MAILBOX_MAX_SIZE_BYTES) ")");

_Static_assert(offsetof(mailbox_t, slots_occupied) == 0, 
               "slots_occupied member offset should be 0");
_Static_assert(offsetof(mailbox_t, slots) == MAILBOX_SLOT_COUNT, 
               "slots member offset should be MAILBOX_SLOT_COUNT");
_Static_assert(offsetof(mailbox_t, slots[1]) == offsetof(mailbox_t, slots[0]) + sizeof(message_slot_t),
               "slots array elements should be packed contiguously");
_Static_assert(sizeof(mailbox_t) % 8 == 0, 
               "mailbox_t size should be 8-byte aligned");
#endif

#ifdef __cplusplus
}
#endif
