/**
 * @file message_id.h
 * @brief Message type definitions
 * @details Defines all message types used in the communication protocol.
 *          These message types are transmission-medium independent and can
 *          be used with any communication layer (shared memory, network, etc.).
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.2
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Message type enumeration
 * @details Defines all possible message types in the communication protocol
 */
typedef enum {
    MSG_INVALID = 0,        /**< Invalid/uninitialized message */
    MSG_PING    = 1,        /**< Ping message for connectivity testing */
    MSG_PONG    = 2,        /**< Pong response to ping */
    MSG_DATA    = 3,        /**< Generic data message */
    MSG_CTRL    = 4,        /**< Control message */
    
    // Command messages (host -> device)
    MSG_CMD_MALLOC           = 5,   /**< Memory allocation request */
    MSG_CMD_FREE             = 6,   /**< Memory free request */
    MSG_CMD_LAUNCH           = 7,   /**< Kernel launch request */
    MSG_CMD_QUERY_DEVICE     = 8,   /**< Device query request */
    MSG_CMD_BEGIN_BATCH      = 9,   /**< Batch begin command */
    MSG_CMD_END_BATCH        = 10,  /**< Batch end command */
    MSG_CMD_TRANSFER_BEGIN   = 11,  /**< Transfer begin command */
    MSG_CMD_TRANSFER_FINISHED = 12, /**< Transfer finished command */
    
    // Response messages (device -> host)
    MSG_RSP_MALLOC           = 13,  /**< Memory allocation response */
    MSG_RSP_FREE             = 14,  /**< Memory free response */
    MSG_RSP_LAUNCH           = 15,  /**< Kernel launch response */
    MSG_RSP_QUERY_DEVICE     = 16,  /**< Device query response */
    MSG_RSP_BEGIN_BATCH      = 17,  /**< Batch begin response */
    MSG_RSP_END_BATCH        = 18,  /**< Batch end response */
    MSG_RSP_TRANSFER_BEGIN   = 19,  /**< Transfer begin response */
    MSG_RSP_TRANSFER_FINISHED = 20, /**< Transfer finished response */
    
    // Internal commands (batch controller -> router)
    MSG_INTERNAL_INVALIDATE_SLOTS = 0x7001,  /**< Invalidate H2D slots */
    
    MSG_VENDOR_BASE = 0x8000u
} message_id_t;  // Changed from msg_id_t to message_id_t

#ifdef __cplusplus
}
#endif
