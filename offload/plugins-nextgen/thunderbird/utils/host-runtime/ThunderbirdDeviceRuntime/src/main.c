/**
 * @file main.c
 * @brief Thunderbird Device Runtime main application
 */

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <string.h>

#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/symbol.h>


// Core includes
#include "ivshmem_dt.h"
#include "mailbox.h"
#include "mailbox_region.h" 
#include "mailbox_operations.h"
#include "ivshmem_heap.h"
#include "message_router.h"
#include "message_slot.h"
#include "error_codes.h"
#include "device_memory.h"
#include "message_router.h"
#include "batch_controller.h"

// Test includes
#include "unit_test_framework.h"

LOG_MODULE_REGISTER(app);

// Export kernel functions for LLEXT modules
// EXPORT_SYMBOL(k_malloc);
// EXPORT_SYMBOL(k_free);

// /**
//  * @brief Run all test suites
//  */
// static void run_all_tests(void)
// {
//     LOG_INF("\n=== Running Test Suites ===");
    
//     // Run unit tests
//     LOG_INF("\n--- Unit Tests ---");
//     int unit_test_result = run_all_unit_tests();
//     if (unit_test_result != 0) {
//         LOG_WRN("Unit tests completed with failures");
//     } else {
//         LOG_INF("All unit tests passed!");
//     }
    
//     // Run integration tests
//     LOG_INF("\n--- Integration Tests ---");
//     // TODO: Add integration test runner when available
    
//     LOG_INF("=== Test Suites Complete ===\n");
// }

/**
 * @brief Send an error response for unhandled messages
 * @param original_msg The original message that couldn't be handled
 */
static void send_error_response(const message_slot_t* original_msg)
{
    // For now, just log the error. In a full implementation,
    // we would send an error response back to the host
    LOG_ERR("Unhandled message type: %d", original_msg->msg_id);
}

/**
 * @brief Main message processing loop
 * @details Continuously monitors the H2D mailbox for incoming messages
 *          and processes them using the message handler
 */
static void message_processing_loop(void)
{
    k_timeout_t timeout = K_MSEC(100);

    while (true) {
        message_slot_t request_msg;
        uint32_t slot_index_received = 0;
        
        if (mailbox_receive(&request_msg, timeout, &slot_index_received)) {
            LOG_DBG("Received message ID %u from slot %u", request_msg.msg_id, slot_index_received);
            
            // Just route - no response handling needed explicitly
            message_router_handle(&request_msg, slot_index_received);
        }
        
        k_msleep(10000);
    }
}


/**
 * @brief Initialize system components
 */
static bool init_system(void)
{
    // Initialize device memory first
    int ret = device_memory_init();
    if (ret != 0) {
        LOG_ERR("Device memory initialization failed: %d", ret);
        return false;
    }
    
    // Now test shared memory allocation
    void *ptr = device_shared_malloc(8);
    if (!ptr) {
        LOG_ERR("Shared memory allocation failed");
        return false;
    }
    device_shared_free(ptr);

    // Initialize mailbox region
    mailbox_region_init(); // void return, FIXME
    if (ret != 0) {
        LOG_ERR("Mailbox region initialization failed: %d", ret);
        return false;
    }
    
    // Verify initialization
    const mailbox_region_t* region = mailbox_region_get();
    if (!region) {
        LOG_ERR("Mailbox region initialization failed");
        return false;
    }
    
    size_t heap_size = device_shared_total_size();
    LOG_INF("System initialized - Heap: %zu bytes", heap_size);
    
    return true;
}

int main(void)
{
    LOG_INF("Thunderbird Device Runtime v0.2");
    
    // Initialize system
    if (!init_system()) {
        LOG_ERR("System initialization failed");
        return -1;
    }
    
    // Run all tests
    // run_all_tests();

    // Start message processing
    LOG_INF("Entering message processing mode");

    // clear out the mailbox
    mailbox_region_reset();
    // clear the batch controller state so noise from unit testing improperly reset doesn't affect the main loop
    batch_controller_reset_state();

    message_processing_loop();
    
    return 0;
}
