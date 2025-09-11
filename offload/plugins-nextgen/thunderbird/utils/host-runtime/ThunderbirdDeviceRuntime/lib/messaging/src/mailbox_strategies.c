#include "mailbox_strategies.h"
#include "message_id.h"
#include "batch_state.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mailbox_strategies, CONFIG_LOG_DEFAULT_LEVEL);

// Receive strategy implementations

bool receive_scan_for_begin_message(const volatile mailbox_t* mbox, 
                                   const receive_strategy_t* strategy, 
                                   uint32_t* slot_index)
{
    const batch_state_t* state = (const batch_state_t*)strategy->state_data;
    if (!state || !batch_state_can_accept_begin(state)) {
        return false;
    }
    
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        if (mbox->slots_occupied[i] == 1 && 
            mbox->slots[i].msg_id == MSG_CMD_BEGIN_BATCH) {
            *slot_index = i;
            LOG_DBG("Found BEGIN message at slot %u", i);
            return true;
        }
    }
    return false;
}

bool receive_scan_for_end_message(const volatile mailbox_t* mbox, 
                                 const receive_strategy_t* strategy, 
                                 uint32_t* slot_index)
{
    const batch_state_t* state = (const batch_state_t*)strategy->state_data;
    if (!state || !batch_state_can_accept_end(state)) {
        LOG_DBG("Cannot scan for END - state machine rejects");
        return false;
    }
    
    for (uint32_t i = 0; i < MAILBOX_SLOT_COUNT; i++) {
        if (mbox->slots_occupied[i] == 1 && 
            mbox->slots[i].msg_id == MSG_CMD_END_BATCH) {
            *slot_index = i;
            LOG_DBG("Found END message at slot %u", i);
            return true;
        }
    }
    return false;
}

bool receive_collect_batch_body(const volatile mailbox_t* mbox, 
                               const receive_strategy_t* strategy, 
                               uint32_t* slot_index)
{
    const batch_state_t* state = (const batch_state_t*)strategy->state_data;
    if (!state || !batch_state_can_accept_body(state)) {
        LOG_DBG("Cannot collect body - state machine rejects");
        return false;
    }
    
    // Use state machine interface to get next expected slot
    uint8_t target_slot = batch_state_get_next_body_slot(state);
    if (target_slot == 0xFF) {
        LOG_DBG("No more body slots expected");
        return false;
    }
    
    // Check if the expected slot has a message
    if (mbox->slots_occupied[target_slot] == 1) {
        *slot_index = target_slot;
        LOG_DBG("Collecting body message from slot %u", target_slot);
        return true;
    }
    
    // Expected message not available yet
    LOG_DBG("Waiting for body message in slot %u", target_slot);
    return false;
}

// Send strategy implementation
bool send_to_specific_slot(const volatile mailbox_t* mbox,
                          const send_strategy_t* strategy,
                          uint32_t slot_index)
{
    if (slot_index >= MAILBOX_SLOT_COUNT) {
        LOG_ERR("Invalid slot index %u for batch response", slot_index);
        return false;
    }
    
    if (mbox->slots_occupied[slot_index] == 1) {
        LOG_DBG("Overwriting occupied slot %u for batch response", slot_index);
    }
    
    LOG_DBG("Sending batch response to slot %u", slot_index);
    return true;
}

