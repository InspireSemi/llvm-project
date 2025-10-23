/**
 * @file ivshmem_abi.h
 * @brief IVSHMEM Application Binary Interface definitions
 * @details Defines the wire protocol and data structures for communication
 *          between host and device over IVSHMEM. Includes ABI versioning,
 *          mailbox layouts, and portable message formats.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "ivshmem_config.h"  // Pull in the base defines

#include "ivshmem_dt.h"
#include "error_codes.h"              // ABI error codes for wire protocol
#include "message_id.h"                // message type definitions
#include "message_payloads.h"         // message payload structures (WIRE ABI)
#include "message_slot.h"             // message_slot_t is part of the wire ABI
#include "mailbox.h"                  // portable (no Zephyr deps)

// Mailbox layout (two queues from start of control region)
#define MBOX_ALIGN IVSHMEM_MIN_ALIGNMENT
#define MBOX_H2D_OFFSET 0u // Starts at base of shared memory region
#define MBOX_D2H_OFFSET (((sizeof(mailbox_t) + (MBOX_ALIGN - 1)) & ~(MBOX_ALIGN - 1)))

// Convenience defines for mailbox_t member offsets
// REMOVED: MAILBOX_HEAD_OFFSET - scalar members no longer exist
// REMOVED: MAILBOX_TAIL_OFFSET - scalar members no longer exist  
// REMOVED: MAILBOX_COUNT_OFFSET - scalar members no longer exist
// REMOVED: MAILBOX_RESERVED_OFFSET - scalar members no longer exist
#define MAILBOX_SLOTS_OCCUPIED_OFFSET    0
#define MAILBOX_SLOTS_OFFSET            MAILBOX_SLOT_COUNT

// Message slot offset defines for each slot in the mailbox
#define SLOT_OFFSET(n) (MAILBOX_SLOTS_OFFSET + ((n) * sizeof(message_slot_t)))

// H2D (Host-to-Device) mailbox member offsets
// REMOVED: MBOX_H2D_HEAD_OFFSET - scalar members no longer exist
// REMOVED: MBOX_H2D_TAIL_OFFSET - scalar members no longer exist
// REMOVED: MBOX_H2D_COUNT_OFFSET - scalar members no longer exist
// REMOVED: MBOX_H2D_RESERVED_OFFSET - scalar members no longer exist
#define MBOX_H2D_SLOTS_OCCUPIED_OFFSET    (MBOX_H2D_OFFSET + MAILBOX_SLOTS_OCCUPIED_OFFSET)
#define MBOX_H2D_SLOTS_OFFSET            (MBOX_H2D_OFFSET + MAILBOX_SLOTS_OFFSET)

// D2H (Device-to-Host) mailbox member offsets
// REMOVED: MBOX_D2H_HEAD_OFFSET - scalar members no longer exist
// REMOVED: MBOX_D2H_TAIL_OFFSET - scalar members no longer exist
// REMOVED: MBOX_D2H_COUNT_OFFSET - scalar members no longer exist
// REMOVED: MBOX_D2H_RESERVED_OFFSET - scalar members no longer exist
#define MBOX_D2H_SLOTS_OCCUPIED_OFFSET    (MBOX_D2H_OFFSET + MAILBOX_SLOTS_OCCUPIED_OFFSET)
#define MBOX_D2H_SLOTS_OFFSET            (MBOX_D2H_OFFSET + MAILBOX_SLOTS_OFFSET)

// H2D mailbox slot offsets
#define MBOX_H2D_SLOT_OFFSET(n)  (MBOX_H2D_OFFSET + SLOT_OFFSET(n))

// D2H mailbox slot offsets
#define MBOX_D2H_SLOT_OFFSET(n)  (MBOX_D2H_OFFSET + SLOT_OFFSET(n))

// Compile-time layout guards (host/device must agree)
#ifdef __cplusplus
// Validate that mailbox layout fits in reasonable control region
static_assert(MBOX_D2H_OFFSET + sizeof(mailbox_t) <= IVSHMEM_CTRL_REGION_MAX_SIZE,
              "Mailbox layout exceeds minimum control region size");
static_assert(MBOX_H2D_OFFSET + sizeof(mailbox_t) <= IVSHMEM_CTRL_REGION_MAX_SIZE,
              "Mailbox layout exceeds minimum control region size");

// Validate that mailboxes don't overlap
static_assert(MBOX_H2D_OFFSET + sizeof(mailbox_t) <= MBOX_D2H_OFFSET,
              "H2D mailbox overlaps with D2H mailbox");
static_assert(MBOX_D2H_OFFSET + sizeof(mailbox_t) <= IVSHMEM_CTRL_REGION_MAX_SIZE,
              "D2H mailbox extends beyond control region");

#else

_Static_assert(MBOX_D2H_OFFSET + sizeof(mailbox_t) <= IVSHMEM_CTRL_REGION_MAX_SIZE,
               "Mailbox layout exceeds minimum control region size");
_Static_assert(MBOX_H2D_OFFSET + sizeof(mailbox_t) <= IVSHMEM_CTRL_REGION_MAX_SIZE,
               "Mailbox layout exceeds minimum control region size");

// Validate that mailboxes don't overlap
_Static_assert(MBOX_H2D_OFFSET + sizeof(mailbox_t) <= MBOX_D2H_OFFSET,
               "H2D mailbox overlaps with D2H mailbox");
_Static_assert(MBOX_D2H_OFFSET + sizeof(mailbox_t) <= IVSHMEM_CTRL_REGION_MAX_SIZE,
               "D2H mailbox extends beyond control region");

#endif

