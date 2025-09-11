#include "batch_controller.h"
#include "message_router.h"
#include "batch_state.h"
#include "mailbox_strategies.h"
#include "message_payloads.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(batch_controller, CONFIG_LOG_DEFAULT_LEVEL);

static batch_state_t batch_state_machine = {0};

// Forward declarations
static error_code_t send_response_via_router(message_id_t response_msg_id, 
                                            const void* response_data, 
                                            uint32_t data_length, 
                                            uint8_t target_slot);

error_code_t batch_controller_init(void)
{
    return batch_state_init(&batch_state_machine);
}

error_code_t batch_controller_handle_begin(const batch_begin_cmd_t* cmd, uint8_t slot_index, const message_slot_t* message_slot)  // ← Add message_slot parameter
{
    if (!cmd || !message_slot) {  // ← Validate message_slot too
        return ERR_INVALID_PARAM;
    }
    
    LOG_DBG("Processing BEGIN command at slot %u", slot_index);
    
    // Create event data with BOTH cmd and message_slot
    batch_begin_event_data_t event_data = {
        .cmd = cmd,
        .slot_index = slot_index,
        .message_slot = message_slot  // ← Pass the complete slot!
    };
    
    return batch_state_transition(&batch_state_machine, BATCH_EVENT_BEGIN_RECEIVED, &event_data);
}

error_code_t batch_controller_handle_end(const batch_end_cmd_t* cmd, uint8_t slot_index, const message_slot_t* message_slot)  // ← Add message_slot parameter
{
    if (!cmd || !message_slot) {  // ← Validate message_slot too
        return ERR_INVALID_PARAM;
    }
    
    LOG_DBG("Processing END command at slot %u", slot_index);
    
    // Create event data with BOTH cmd and message_slot
    batch_end_event_data_t event_data = {
        .cmd = cmd,
        .slot_index = slot_index,
        .message_slot = message_slot  // ← Pass the complete slot!
    };
    
    return batch_state_transition(&batch_state_machine, BATCH_EVENT_END_RECEIVED, &event_data);
}

error_code_t batch_controller_handle_body_message(const message_slot_t* slot, uint8_t slot_index)
{
    if (!slot) {
        return ERR_INVALID_PARAM;
    }
    
    body_event_data_t event_data = {
        .message = slot,
        .slot_index = slot_index
    };
    
    // Get state before processing the body message
    batch_state_enum_t state_before = batch_state_get_current(&batch_state_machine);
    
    // Check if this will be the last body message BEFORE processing
    const batch_context_t* context = batch_state_get_context(&batch_state_machine);
    bool will_be_complete = (context && (context->collected_body_count + 1) >= context->expected_body_count);
    
    error_code_t result = batch_state_transition(&batch_state_machine, BATCH_EVENT_BODY_RECEIVED, &event_data);
    
    // Get state after processing
    batch_state_enum_t state_after = batch_state_get_current(&batch_state_machine);
    
    LOG_DBG("Body message processing: state %d -> %d (will_be_complete=%s)", 
            state_before, state_after, will_be_complete ? "true" : "false");
    
    // Check if this was the completing message
    if (result == ERR_OK && will_be_complete) {
        LOG_INF("Batch is ready to process - starting execution");
        
        // Transition to processing state
        error_code_t process_transition = batch_state_transition(&batch_state_machine, BATCH_EVENT_PROCESSING_STARTED, NULL);
        if (process_transition != ERR_OK) {
            LOG_ERR("Failed to transition to processing state: %d", process_transition);
            return process_transition;
        }
        
        // Execute batch processing
        error_code_t process_result = batch_controller_complete_batch();
        
        // Mark processing complete or error
        if (process_result == ERR_OK) {
            batch_state_transition(&batch_state_machine, BATCH_EVENT_PROCESSING_COMPLETE, NULL);
            LOG_INF("Batch processing completed successfully");
            
            // Automatically transition to IDLE state for next batch
            LOG_DBG("Auto-resetting batch state after successful completion");
            batch_state_transition(&batch_state_machine, BATCH_EVENT_RESET, NULL);
            LOG_INF("Batch state reset to IDLE, ready for next batch");
        } else {
            batch_state_transition(&batch_state_machine, BATCH_EVENT_ERROR, NULL);
            LOG_ERR("Batch processing failed: %d", process_result);
        }
        
        return process_result;
    }
    
    return result;
}

const batch_state_t* batch_controller_peek_batch_state(void)
{
    return &batch_state_machine;
}

bool batch_controller_is_batch_active(void)
{
    return batch_state_is_active(&batch_state_machine);
}

error_code_t batch_controller_reset_state(void)
{
    LOG_DBG("Manual batch state reset requested");
    return batch_state_transition(&batch_state_machine, BATCH_EVENT_RESET, NULL);
}

send_strategy_t batch_controller_get_send_strategy(void)
{
    send_strategy_t strategy = {0};
    batch_state_enum_t current_state = batch_state_get_current(&batch_state_machine);
    
    switch (current_state) {
        case BATCH_STATE_IDLE:
        case BATCH_STATE_WAITING_FOR_END:
        case BATCH_STATE_COLLECTING_BODY:
        case BATCH_STATE_READY_TO_PROCESS:
        case BATCH_STATE_PROCESSING:
        case BATCH_STATE_COMPLETE:
            // For all states, use specific slot addressing
            strategy.send_to_slot_fn = send_to_specific_slot;
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: Send to specific slot");
            break;
            
        case BATCH_STATE_ERROR:
        default:
            // Even in error state, maintain send capability
            strategy.send_to_slot_fn = send_to_specific_slot;
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: Send to specific slot (error state)");
            break;
    }
    
    return strategy;
}

receive_strategy_t batch_controller_get_receive_strategy(void)
{
    receive_strategy_t strategy = {0};
    batch_state_enum_t current_state = batch_state_get_current(&batch_state_machine);
    
    switch (current_state) {
        case BATCH_STATE_IDLE:
            // Looking for BEGIN message only
            strategy.find_slot_fn = receive_scan_for_begin_message;
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: Scanning for BEGIN message");
            break;
            
        case BATCH_STATE_COMPLETE:
            // COMPLETE state should transition to IDLE immediately
            // No messages should be processed in this brief state
            strategy.find_slot_fn = NULL;
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: COMPLETE state - no new messages accepted");
            break;
            
        case BATCH_STATE_WAITING_FOR_END:
            // Have BEGIN, looking for END message only
            strategy.find_slot_fn = receive_scan_for_end_message;
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: Scanning for END message");
            break;
            
        case BATCH_STATE_COLLECTING_BODY:
            // Have BEGIN and END, collecting body messages only
            strategy.find_slot_fn = receive_collect_batch_body;
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: Collecting body messages");
            break;
            
        case BATCH_STATE_READY_TO_PROCESS:
        case BATCH_STATE_PROCESSING:
            // Batch processing - no new messages accepted
            strategy.find_slot_fn = NULL; // No strategy during processing
            strategy.state_data = &batch_state_machine;
            LOG_DBG("Strategy: Processing batch, no new messages");
            break;
            
        case BATCH_STATE_ERROR:
        default:
            // Error state - reset and look for BEGIN
            LOG_WRN("Batch state machine in error state, resetting");
            LOG_DBG("Error state recovery reset");
            batch_state_transition(&batch_state_machine, BATCH_EVENT_RESET, NULL);
            strategy.find_slot_fn = receive_scan_for_begin_message;
            strategy.state_data = &batch_state_machine;
            break;
    }
    
    return strategy;
}

/**
 * @brief Send response via router using slot_message_received_at as target slot
 */
static error_code_t send_response_via_router(message_id_t response_msg_id, 
                                            const void* response_data, 
                                            uint32_t data_length, 
                                            uint8_t target_slot)
{
    message_slot_t response_msg = {0};
    
    response_msg.msg_id = response_msg_id;
    response_msg.length = data_length;
    
    if (data_length > 0 && response_data) {
        if (data_length > MESSAGE_SLOT_DATA_SIZE) {
            LOG_ERR("Response data too large: %u > %u", data_length, MESSAGE_SLOT_DATA_SIZE);
            return ERR_INVALID_PARAM;
        }
        memcpy(response_msg.data, response_data, data_length);
    }
    
    // Use the correct message_router_handle signature
    bool success = message_router_handle(&response_msg, target_slot);
    
    return success ? ERR_OK : ERR_IO;
}

/**
 * @brief Send slot invalidation command via router
 */
static error_code_t send_invalidate_slots_via_router(const uint8_t* slots, uint8_t slot_count)
{
    message_slot_t invalidate_msg = {0};
    internal_invalidate_slots_cmd_t cmd = {0};
    
    cmd.slot_count = slot_count;
    if (slot_count > 0 && slots) {
        if (slot_count > MAX_BATCH_SLOTS) {
            LOG_ERR("Too many slots to invalidate: %u > %u", slot_count, MAX_BATCH_SLOTS);
            return ERR_INVALID_PARAM;
        }
        memcpy(cmd.slots, slots, slot_count);
    }
    
    invalidate_msg.msg_id = MSG_INTERNAL_INVALIDATE_SLOTS;
    invalidate_msg.length = sizeof(cmd);
    memcpy(invalidate_msg.data, &cmd, sizeof(cmd));
    
    // Use slot 0 as dummy - router recognizes internal command by msg_id
    bool success = message_router_handle(&invalidate_msg, 0);
    
    return success ? ERR_OK : ERR_IO;
}

/**
 * @brief Process all body messages and send responses via router
 */
static error_code_t process_batch_body_messages(void)
{
    const batch_context_t* context = batch_state_get_context(&batch_state_machine);
    if (!context) {
        LOG_ERR("No batch context available");
        return ERR_INVALID_BATCH_STATE;
    }

    const batch_begin_cmd_t* begin_cmd = (const batch_begin_cmd_t*)
        context->batch_buffer.slots[context->begin_message_slot].data;
    
    if (!begin_cmd) {
        LOG_ERR("Missing begin command data");
        return ERR_INVALID_BATCH_STATE;
    }

    LOG_DBG("Processing batch: BEGIN slot=%u, END slot=%u, total slots=%u", 
            context->begin_message_slot, context->end_message_slot, begin_cmd->slot_count);

    for (uint8_t i = 0; i < begin_cmd->slot_count; i++) {
        uint8_t slot_idx = begin_cmd->batch_slots[i];
        
        LOG_DBG("Processing slot %u (index %u): BEGIN=%u, END=%u", 
                slot_idx, i, context->begin_message_slot, context->end_message_slot);
        
        // Skip BEGIN/END slots
        if (slot_idx == context->begin_message_slot || 
            slot_idx == context->end_message_slot) {
            LOG_DBG("Skipping protocol slot %u", slot_idx);
            continue;
        }

        const message_slot_t* body_slot = &context->batch_buffer.slots[slot_idx];
        LOG_DBG("Processing body message in slot %u, msg_id=%u", slot_idx, body_slot->msg_id);
        
        message_slot_t response = {0};
        
        // Use correct message router function for business logic processing
        error_code_t router_result = message_router_generate_response(body_slot, &response);
        if (router_result == ERR_OK) {
            LOG_DBG("Generated response msg_id=%u for slot %u", response.msg_id, slot_idx);
            
            // Send response via router - slot_idx tells router where to send it
            bool send_success = message_router_handle(&response, slot_idx);
                
            if (!send_success) {
                LOG_ERR("Failed to send response for slot %u", slot_idx);
            } else {
                LOG_DBG("Successfully sent response for slot %u", slot_idx);
            }
        } else {
            LOG_ERR("Failed to generate response for body message in slot %u: %d", slot_idx, router_result);
        }
    }

    return ERR_OK;
}

/**
 * @brief Send protocol responses via router
 */
static error_code_t send_protocol_responses(void)
{
    const batch_context_t* context = batch_state_get_context(&batch_state_machine);
    if (!context) {
        return ERR_INVALID_BATCH_STATE;
    }

    // Send BEGIN response
    const batch_begin_cmd_t* begin_cmd = (const batch_begin_cmd_t*)
        context->batch_buffer.slots[context->begin_message_slot].data;
    
    if (begin_cmd) {
        batch_begin_rsp_t response_begin = {0};
        response_begin.slot_count = begin_cmd->slot_count;
        memcpy(response_begin.batch_slots, begin_cmd->batch_slots, sizeof(response_begin.batch_slots));

        send_response_via_router(MSG_RSP_BEGIN_BATCH, &response_begin, 
                                sizeof(response_begin), context->begin_message_slot);
    }

    // Send END response
    const batch_end_cmd_t* end_cmd = (const batch_end_cmd_t*)
        context->batch_buffer.slots[context->end_message_slot].data;
    
    if (end_cmd) {
        batch_end_rsp_t response_end = {0};
        response_end.slot_count = end_cmd->slot_count;
        memcpy(response_end.batch_slots, end_cmd->batch_slots, sizeof(response_end.batch_slots));

        send_response_via_router(MSG_RSP_END_BATCH, &response_end, 
                                sizeof(response_end), context->end_message_slot);
    }

    return ERR_OK;
}

/**
 * @brief Send cleanup command to router
 * @details Invalidates H2D slots to signal host that device has consumed the batch messages.
 *          This must be done before sending protocol responses to ensure proper ordering.
 */
static error_code_t cleanup_batch_slots(void)
{
    const batch_context_t* context = batch_state_get_context(&batch_state_machine);
    if (!context) {
        return ERR_INVALID_BATCH_STATE;
    }

    const batch_begin_cmd_t* begin_cmd = (const batch_begin_cmd_t*)
        context->batch_buffer.slots[context->begin_message_slot].data;
    
    if (!begin_cmd) {
        return ERR_INVALID_BATCH_STATE;
    }

    LOG_INF("Invalidating %u H2D slots before sending protocol responses", begin_cmd->slot_count);
    return send_invalidate_slots_via_router(begin_cmd->batch_slots, begin_cmd->slot_count);
}

/**
 * @brief Complete batch processing - now just orchestration!
 */
error_code_t batch_controller_complete_batch(void)
{
    const batch_context_t* context = batch_state_get_context(&batch_state_machine);
    if (!context) {
        LOG_ERR("No batch context available");
        return ERR_INVALID_BATCH_STATE;
    }

    LOG_INF("Processing batch with %u messages", context->total_messages);

    // Phase 1: Process all body messages
    error_code_t result = process_batch_body_messages();
    if (result != ERR_OK) {
        LOG_ERR("Failed to process batch body messages: %d", result);
        return result;
    }

    // Phase 2: Invalidate H2D slots before sending protocol responses
    // This ensures host slots are cleaned up before batch end response indicates completion
    cleanup_batch_slots();

    // Phase 3: Send protocol responses
    send_protocol_responses();

    LOG_INF("Batch processing completed successfully");
    return ERR_OK;
}