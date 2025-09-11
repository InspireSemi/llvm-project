/**
 * @file message_slot.h
 * @brief Message slot data structure definition
 * @details Defines the message slot structure used within mailboxes for
 *          IVSHMEM communication. Includes message ID, length, data payload,
 *          and checksum member (unused for integrity verification).
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#include "ivshmem_config.h"  // Instead of relying on ABI header
#include "message_id.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MESSAGE_SLOT_DATA_SIZE
#error "MESSAGE_SLOT_DATA_SIZE must be defined before including message_slot.h"
#endif

#ifndef MESSAGE_SLOT_MAX_SIZE_BYTES
#error "MESSAGE_SLOT_MAX_SIZE_BYTES must be defined before including message_slot.h"
#endif

// #FIXME the layout and alignment don't work well together. We need the data member to have aligned access and probably should pad after the msg_id field.
typedef struct __attribute__((__packed__, aligned(8))) {
    message_id_t msg_id;  // Change from msg_id_t
    uint32_t length;      // Length of the data payload
    uint8_t data[MESSAGE_SLOT_DATA_SIZE];  // Data payload
    uint32_t checksum;    // Simple checksum for integrity
} message_slot_t;

// Static assertions for structure layout and size verification
#ifdef __cplusplus
// C++ static assertions
static_assert(sizeof(message_slot_t) <= MESSAGE_SLOT_MAX_SIZE_BYTES, 
              "message_slot_t size must not exceed MESSAGE_SLOT_MAX_SIZE_BYTES");
static_assert(offsetof(message_slot_t, msg_id) == 0, 
              "msg_id must be at offset 0");
static_assert(offsetof(message_slot_t, length) == sizeof(message_id_t), 
              "length must immediately follow msg_id");
static_assert(offsetof(message_slot_t, data) == sizeof(message_id_t) + sizeof(uint32_t), 
              "data must immediately follow length");
static_assert(offsetof(message_slot_t, checksum) == sizeof(message_id_t) + sizeof(uint32_t) + MESSAGE_SLOT_DATA_SIZE, 
              "checksum must be at end of structure");
static_assert(offsetof(message_slot_t, data[1]) == offsetof(message_slot_t, data[0]) + sizeof(uint8_t),
              "data array elements should be packed contiguously");
static_assert(alignof(message_slot_t) == 8, 
              "message_slot_t must be 8-byte aligned");
#else
// C11 static assertions (requires stddef.h for offsetof)
_Static_assert(sizeof(message_slot_t) <= MESSAGE_SLOT_MAX_SIZE_BYTES, 
               "message_slot_t size must not exceed MESSAGE_SLOT_MAX_SIZE_BYTES");
_Static_assert(offsetof(message_slot_t, msg_id) == 0, 
               "msg_id must be at offset 0");
_Static_assert(offsetof(message_slot_t, length) == sizeof(message_id_t), 
               "message_slot_t.length not at expected offset");
_Static_assert(offsetof(message_slot_t, data) == sizeof(message_id_t) + sizeof(uint32_t), 
               "message_slot_t.data not at expected offset");
_Static_assert(offsetof(message_slot_t, checksum) == sizeof(message_id_t) + sizeof(uint32_t) + MESSAGE_SLOT_DATA_SIZE, 
               "message_slot_t.checksum not at expected offset");
_Static_assert(offsetof(message_slot_t, data[1]) == offsetof(message_slot_t, data[0]) + sizeof(uint8_t),
               "data array elements should be packed contiguously");
_Static_assert(_Alignof(message_slot_t) == 8, 
               "message_slot_t must be 8-byte aligned");
#endif

#ifdef __cplusplus
}
#endif
