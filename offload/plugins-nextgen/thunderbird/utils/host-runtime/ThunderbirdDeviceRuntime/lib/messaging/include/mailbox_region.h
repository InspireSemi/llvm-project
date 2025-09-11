/**
 * @file mailbox_region.h
 * @brief IVSHMEM mailbox region management interface
 * @details Provides initialization and access functions for host-to-device
 *          and device-to-host mailboxes located within the IVSHMEM control
 *          region. Manages the complete region including paired operations.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include "mailbox.h"
#include "message_slot.h"

/**
 * @brief Mailbox region structure containing both communication directions
 */
typedef struct {
    volatile mailbox_t* h2d;  ///< Host-to-device mailbox
    volatile mailbox_t* d2h;  ///< Device-to-host mailbox
} mailbox_region_t;

/**
 * @brief Simplified region status structure
 */
typedef struct {
    bool h2d_has_messages;  ///< True if H2D mailbox has messages
    bool h2d_has_space;     ///< True if H2D mailbox has space
    bool d2h_has_messages;  ///< True if D2H mailbox has messages 
    bool d2h_has_space;     ///< True if D2H mailbox has space
} mailbox_region_status_t;

/**
 * @brief Initialize and reset the two mailboxes within the IVSHMEM region
 * @details Clears both host-to-device and device-to-host mailboxes to ensure
 *          a clean starting state for communication
 */
void mailbox_region_init(void);

/**
 * @brief Get the mailbox region containing both H2D and D2H mailboxes
 * @return Pointer to the mailbox region structure
 */
const mailbox_region_t* mailbox_region_get(void);

/**
 * @brief Get pointer to the host-to-device mailbox
 * @return Volatile pointer to the host-to-device mailbox structure
 */
volatile mailbox_t* mailbox_region_h2d(void);

/**
 * @brief Get pointer to the device-to-host mailbox
 * @return Volatile pointer to the device-to-host mailbox structure  
 */
volatile mailbox_t* mailbox_region_d2h(void);

/**
 * @brief Get byte offset of host-to-device mailbox from IVSHMEM base
 * @return Offset in bytes (for debug/host mirroring)
 */
size_t mailbox_region_h2d_offset(void);

/**
 * @brief Get byte offset of device-to-host mailbox from IVSHMEM base
 * @return Offset in bytes (for debug/host mirroring)
 */
size_t mailbox_region_d2h_offset(void);

/**
 * @brief Get status of both mailboxes in the region
 * @param h2d_count Pointer to store H2D mailbox message count (optional)
 */

/**
 * @brief Get status of both mailboxes in a simple structure
 * @return Status structure with current state of both mailboxes
 */
mailbox_region_status_t mailbox_region_get_status(void);

/**
 * @brief Reset mailbox region to initial state
 * @details Clears both H2D and D2H mailboxes
 */
void mailbox_region_reset(void);
