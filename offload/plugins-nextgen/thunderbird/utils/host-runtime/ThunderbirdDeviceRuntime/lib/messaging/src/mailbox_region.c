/**
 * @file mailbox_region.c
 * @brief IVSHMEM mailbox region management implementation
 * @details Implements mailbox region initialization, management, and region-level
 *          operations that work with both H2D and D2H mailboxes as a coordinated pair.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-08-09
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "../include/mailbox_region.h"
#include "../include/mailbox_operations.h"
#include "../../common/include/tx_media_dependent_defines.h"
#include <string.h>
#include <stdint.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mailbox_region, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT((TX_MBOX_D2H_OFFSET + sizeof(mailbox_t)) <= TX_CTRL_REGION_SIZE,
             "Two mailboxes must fit within transmission control region");

// Static pointers to the mailboxes in shared memory (exposed for testing)
volatile mailbox_t *const mbox_h2d_ptr =
    (volatile mailbox_t *)(uintptr_t)TX_MBOX_H2D_ADDR;
volatile mailbox_t *const mbox_d2h_ptr =
    (volatile mailbox_t *)(uintptr_t)TX_MBOX_D2H_ADDR;

// Global mailbox region instance
static mailbox_region_t g_mailbox_region;
// Exposed for testing
bool g_region_initialized = false;

/**
 * @brief Reset a mailbox to its initial empty state
 * @note STUB: Scalar members removed - needs alternative implementation
 * @todo Implement using slots_occupied array to track slot usage
 * @param mb Pointer to the mailbox structure to reset
 */
static inline void mbox_reset(volatile mailbox_t *mb)
{
    if (mb != NULL) {
        // Clear both the slots_occupied tracking array and slots array
        memset((void*)mb->slots_occupied, 0, sizeof(mb->slots_occupied));
        memset((void*)mb->slots, 0, sizeof(mb->slots));
    }
}

void mailbox_region_init(void)
{
    // Reset both mailboxes
    mbox_reset(mbox_h2d_ptr);
    mbox_reset(mbox_d2h_ptr);
    
    // Initialize the region structure
    g_mailbox_region.h2d = mbox_h2d_ptr;
    g_mailbox_region.d2h = mbox_d2h_ptr;
    g_region_initialized = true;
    
    LOG_INF("Mailbox region initialized at H2D=0x%08x, D2H=0x%08x", 
            (uint32_t)(uintptr_t)mbox_h2d_ptr, (uint32_t)(uintptr_t)mbox_d2h_ptr);
}

const mailbox_region_t* mailbox_region_get(void)
{
    if (!g_region_initialized) {
        LOG_WRN("Mailbox region not initialized, initializing now");
        mailbox_region_init();
    }
    
    return &g_mailbox_region;
}

volatile mailbox_t* mailbox_region_h2d(void)
{
    return mbox_h2d_ptr;
}

volatile mailbox_t* mailbox_region_d2h(void)
{
    return mbox_d2h_ptr;
}

size_t mailbox_region_h2d_offset(void)
{
    return tx_get_h2d_offset();
}

size_t mailbox_region_d2h_offset(void)
{
    return tx_get_d2h_offset();
}

mailbox_region_status_t mailbox_region_get_status(void)
{
    mailbox_region_status_t status = {0};
    
    if (g_region_initialized && g_mailbox_region.h2d && g_mailbox_region.d2h) {
        status.h2d_has_messages = mailbox_has_messages(g_mailbox_region.h2d);
        status.h2d_has_space = mailbox_has_space(g_mailbox_region.h2d);
        status.d2h_has_messages = mailbox_has_messages(g_mailbox_region.d2h);
        status.d2h_has_space = mailbox_has_space(g_mailbox_region.d2h);
    }
    
    return status;
}

void mailbox_region_reset(void)
{
    if (!g_region_initialized) {
        LOG_WRN("Mailbox region not initialized");
        return;
    }
    
    mailbox_reset(g_mailbox_region.h2d);
    mailbox_reset(g_mailbox_region.d2h);
    
    LOG_INF("Mailbox region reset completed");
}
