/**
 * @file message_router.c
 * @brief Message routing and processing implementation
 */

#include "message_router.h"
#include "message_payloads.h"
#include "device_memory.h"
#include "batch_controller.h"
#include "error_codes.h"
#include "kernel_processor.h"
#include "mailbox_operations.h"
#include "memory_command_handler.h"
#include "memory_response_builder.h"
#include "kernel_command_handler.h"
#include "device_command_handler.h"
#include "device_response_builder.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(message_router, LOG_LEVEL_DBG);

/**
 * @brief Handle internal invalidate slots command
 */
static bool handle_internal_invalidate_slots(const message_slot_t* slot, uint8_t slot_received_at)
{
    const internal_invalidate_slots_cmd_t* cmd = (const internal_invalidate_slots_cmd_t*)slot->data;
    
    if (slot->length < sizeof(internal_invalidate_slots_cmd_t)) {
        LOG_ERR("Invalid internal invalidate slots command size");
        return false;
    }
    
    LOG_INF("Invalidating %u H2D slots (device has consumed batch messages)", cmd->slot_count);
    
    for (uint8_t i = 0; i < cmd->slot_count; i++) {
        error_code_t result = mailbox_operations_invalidate_h2d_slot(cmd->slots[i]);
        if (result != ERR_OK) {
            LOG_WRN("Failed to invalidate H2D slot %u: %d", cmd->slots[i], result);
        } else {
            LOG_DBG("Invalidated H2D slot %u", cmd->slots[i]);
        }
    }
    
    LOG_INF("H2D slot invalidation complete");
    return true;
}

/**
 * @brief Send response message to specific slot
 */
static bool send_response_to_slot(const message_slot_t* response_msg, uint8_t target_slot)
{
    error_code_t result = mailbox_operations_send_message_to_slot(
        response_msg->msg_id,
        response_msg->data,
        response_msg->length,
        target_slot);
        
    if (result != ERR_OK) {
        LOG_ERR("Failed to send response to slot %u: %d", target_slot, result);
        return false;
    }
    
    LOG_DBG("Sent response message %u to slot %u", response_msg->msg_id, target_slot);
    return true;
}

/**
 * @brief Handle normal (non-batch) messages
 */
static bool handle_normal_message(const message_slot_t* slot, uint8_t slot_message_received_at)
{
    LOG_DBG("Processing normal message ID %u", slot->msg_id);
    // Handle non-batch messages here if needed
    // For now, we only support batch processing
    return true;
}

/**
 * @brief Handle batch assembly messages with state machine validation
 */
static bool handle_batch_assembly(const message_slot_t* slot, uint8_t slot_message_received_at)
{
    LOG_DBG("Processing batch assembly message ID %u at slot %u", slot->msg_id, slot_message_received_at);
    
    const batch_state_t* batch_state = batch_controller_peek_batch_state();
    if (!batch_state) {
        LOG_ERR("No batch state available");
        return false;
    }
    
    batch_state_enum_t current_state = batch_state_get_current(batch_state);
    LOG_DBG("Current batch state: %d", current_state);
    
    switch (slot->msg_id) {
        case MSG_CMD_BEGIN_BATCH: {
            LOG_DBG("BEGIN message received, checking if state allows it");
            
            bool can_accept = batch_state_can_accept_begin(batch_state);
            LOG_DBG("batch_state_can_accept_begin() returned: %s", can_accept ? "true" : "false");
            
            if (!can_accept) {
                LOG_ERR("BEGIN message rejected in state: %d (expected IDLE=%d)", 
                        current_state, BATCH_STATE_IDLE);
                return false;
            }
            
            const batch_begin_cmd_t* cmd = (const batch_begin_cmd_t*)slot->data;
            error_code_t result = batch_controller_handle_begin(cmd, slot_message_received_at, slot);  // ← Pass slot!
            return (result == ERR_OK);
        }
        
        case MSG_CMD_END_BATCH: {
            LOG_DBG("END message received, checking if state allows it");
            
            bool can_accept = batch_state_can_accept_end(batch_state);
            LOG_DBG("batch_state_can_accept_end() returned: %s", can_accept ? "true" : "false");
            
            if (!can_accept) {
                LOG_ERR("END message rejected in state: %d (expected WAITING_FOR_END=%d)", 
                        current_state, BATCH_STATE_WAITING_FOR_END);
                return false;
            }
            
            const batch_end_cmd_t* cmd = (const batch_end_cmd_t*)slot->data;
            error_code_t result = batch_controller_handle_end(cmd, slot_message_received_at, slot);  // ← Pass slot!
            return (result == ERR_OK);
        }
        
        default: {
            LOG_DBG("Body message received, checking if state allows it");
            
            bool can_accept = batch_state_can_accept_body(batch_state);
            LOG_DBG("batch_state_can_accept_body() returned: %s", can_accept ? "true" : "false");
            
            if (!can_accept) {
                LOG_ERR("Body message rejected in state: %d (expected COLLECTING_BODY=%d)", 
                        current_state, BATCH_STATE_COLLECTING_BODY);
                return false;
            }
            
            error_code_t result = batch_controller_handle_body_message(slot, slot_message_received_at);
            return (result == ERR_OK);
        }
    }
}

/**
 * @brief Generate response for a command message (business logic processing)
 */
error_code_t message_router_generate_response(const message_slot_t* input, message_slot_t* output)
{
    if (!input || !output) {
        return ERR_INVALID_PARAM;
    }
    
    // Clear output
    memset(output, 0, sizeof(message_slot_t));
    
    // Process based on message type
    switch (input->msg_id) {
        case MSG_CMD_MALLOC: {
            malloc_result_t result = {0};
            error_code_t handler_status = handle_malloc_command((malloc_cmd_t*)input->data, &result);
            
            output->msg_id = MSG_RSP_MALLOC;
            output->length = sizeof(malloc_rsp_t);
            error_code_t build_status = build_malloc_response(&result, (malloc_rsp_t*)output->data);
            
            LOG_DBG("Generated MALLOC response");
            return (handler_status == ERR_OK && build_status == ERR_OK) ? ERR_OK : handler_status;
        }
            
        case MSG_CMD_FREE: {
            error_code_t free_status = handle_free_command((free_cmd_t*)input->data);
            
            output->msg_id = MSG_RSP_FREE;
            output->length = sizeof(free_rsp_t);
            error_code_t build_status = build_free_response(free_status, (free_rsp_t*)output->data);
            
            LOG_DBG("Generated FREE response");
            return (free_status == ERR_OK && build_status == ERR_OK) ? ERR_OK : free_status;
        }
            
        case MSG_CMD_LAUNCH: {
            launch_result_t result = {0};
            error_code_t handler_status = handle_launch_command((launch_cmd_t*)input->data, &result);
            
            output->msg_id = MSG_RSP_LAUNCH;
            output->length = sizeof(launch_rsp_t);
            error_code_t build_status = build_launch_response(&result, (launch_rsp_t*)output->data);
            
            LOG_DBG("Generated LAUNCH response");
            return (handler_status == ERR_OK && build_status == ERR_OK) ? ERR_OK : handler_status;
        }
            
        case MSG_CMD_QUERY_DEVICE: {
            query_result_t result = {0};
            error_code_t handler_status = handle_query_device_command((query_device_cmd_t*)input->data, &result);
            
            output->msg_id = MSG_RSP_QUERY_DEVICE;
            output->length = sizeof(query_device_rsp_t);
            error_code_t build_status = build_query_device_response(&result, (query_device_rsp_t*)output->data);
            
            LOG_DBG("Generated QUERY_DEVICE response");
            return (handler_status == ERR_OK && build_status == ERR_OK) ? ERR_OK : handler_status;
        }

        default:
            LOG_WRN("Unknown command type for response generation: %u", input->msg_id);
            return ERR_INVALID_PARAM;
    }
    
    return ERR_OK;
}

/**
 * @brief Main message router entry point
 */
bool message_router_handle(const message_slot_t* slot, uint8_t slot_message_received_at)
{
    if (!slot) {
        LOG_ERR("Invalid parameters to message router");
        return false;
    }

    LOG_DBG("Routing message ID %u from/to slot %u", slot->msg_id, slot_message_received_at);

    // Handle internal commands and responses first
    switch (slot->msg_id) {
        case MSG_INTERNAL_INVALIDATE_SLOTS:
            return handle_internal_invalidate_slots(slot, slot_message_received_at);
            
        case MSG_RSP_BEGIN_BATCH:
        case MSG_RSP_END_BATCH:
        case MSG_RSP_MALLOC:
        case MSG_RSP_FREE:
        case MSG_RSP_LAUNCH:
        case MSG_RSP_QUERY_DEVICE:
        case MSG_RSP_TRANSFER_BEGIN:
        case MSG_RSP_TRANSFER_FINISHED:
            return send_response_to_slot(slot, slot_message_received_at);
            
        default:
            break;
    }

    // Route incoming command messages using state machine
    if (slot->msg_id == MSG_CMD_BEGIN_BATCH || 
        slot->msg_id == MSG_CMD_END_BATCH ||
        batch_controller_is_batch_active()) {
        
        return handle_batch_assembly(slot, slot_message_received_at);
    } else {
        return handle_normal_message(slot, slot_message_received_at);
    }
}
