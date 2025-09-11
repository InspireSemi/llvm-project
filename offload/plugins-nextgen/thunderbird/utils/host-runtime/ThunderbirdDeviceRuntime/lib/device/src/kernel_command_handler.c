#include "kernel_command_handler.h"
#include "kernel_processor.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kernel_command_handler, LOG_LEVEL_DBG);

error_code_t handle_launch_command(const launch_cmd_t* cmd, launch_result_t* result)
{
    if (!cmd || !result) {
        return ERR_INVALID_PARAM;
    }

    LOG_DBG("Handling launch command: kernel=0x%llx, grid=(%u,%u,%u), block=(%u,%u,%u)", 
            cmd->kernel_address, cmd->grid_x, cmd->grid_y, cmd->grid_z,
            cmd->block_x, cmd->block_y, cmd->block_z);

    // Validate parameters
    if (cmd->kernel_address == 0) {
        LOG_ERR("Invalid kernel address: 0");
        result->status = ERR_INVALID_PARAM;
        result->launch_id = 0;
        return ERR_INVALID_PARAM;
    }

    if (cmd->grid_x == 0 || cmd->grid_y == 0 || cmd->grid_z == 0 ||
        cmd->block_x == 0 || cmd->block_y == 0 || cmd->block_z == 0) {
        LOG_ERR("Invalid grid or block dimensions");
        result->status = ERR_INVALID_PARAM;
        result->launch_id = 0;
        return ERR_INVALID_PARAM;
    }

    // Execute kernel launch
    error_code_t launch_result_code = kernel_processor_execute(cmd);
    
    result->status = launch_result_code;
    result->launch_id = 0;  // Could be assigned by kernel_processor_execute if needed
    
    if (launch_result_code == ERR_OK) {
        LOG_DBG("Kernel launch succeeded");
    } else {
        LOG_ERR("Kernel launch failed: %d", launch_result_code);
    }

    return launch_result_code;
}
