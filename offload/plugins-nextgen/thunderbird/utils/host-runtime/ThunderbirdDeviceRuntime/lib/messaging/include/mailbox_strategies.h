#ifndef MAILBOX_STRATEGIES_H
#define MAILBOX_STRATEGIES_H

#include "mailbox.h"
#include "message_slot.h"
#include "batch_state.h"

// Forward declarations
struct receive_strategy;
struct send_strategy;

/**
 * @brief Function pointer for finding the next slot to receive from
 * @param mbox Source mailbox
 * @param strategy Strategy containing state data
 * @param slot_index Output parameter for found slot index
 * @return true if suitable slot found, false otherwise
 */
typedef bool (*receive_slot_finder_fn_t)(const volatile mailbox_t* mbox, 
                                         const struct receive_strategy* strategy, 
                                         uint32_t* slot_index);

/**
 * @brief Function pointer for sending to a specific slot
 * @param mbox Target mailbox
 * @param strategy Strategy containing state data
 * @param slot_index Target slot index to send to
 * @return true if send successful, false otherwise
 */
typedef bool (*send_to_slot_fn_t)(const volatile mailbox_t* mbox,
                                  const struct send_strategy* strategy,
                                  uint32_t slot_index);

/**
 * @brief Receive strategy configuration
 */
typedef struct receive_strategy {
    receive_slot_finder_fn_t find_slot_fn;   ///< Function to find next receive slot
    void* state_data;                         ///< Points to batch_state_t or other state
} receive_strategy_t;

/**
 * @brief Send strategy configuration - always uses specific slots for batch responses
 */
typedef struct send_strategy {
    send_to_slot_fn_t send_to_slot_fn;       ///< Function to send to specific slot
    void* state_data;                         ///< Points to batch_state_t
} send_strategy_t;

// Receive strategy implementations
bool receive_scan_for_begin_message(const volatile mailbox_t* mbox, 
                                   const receive_strategy_t* strategy, 
                                   uint32_t* slot_index);

bool receive_scan_for_end_message(const volatile mailbox_t* mbox, 
                                 const receive_strategy_t* strategy, 
                                 uint32_t* slot_index);

bool receive_collect_batch_body(const volatile mailbox_t* mbox, 
                               const receive_strategy_t* strategy, 
                               uint32_t* slot_index);

// Send strategy implementation
bool send_to_specific_slot(const volatile mailbox_t* mbox,
                          const send_strategy_t* strategy,
                          uint32_t slot_index);

#endif // MAILBOX_STRATEGIES_H