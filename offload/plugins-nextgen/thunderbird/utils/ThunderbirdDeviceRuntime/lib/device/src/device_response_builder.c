#include "device_response_builder.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(device_response_builder, LOG_LEVEL_DBG);

error_code_t build_launch_response(const launch_result_t* result, launch_rsp_t* response)
{
    if (!result || !response) {
        return ERR_INVALID_PARAM;
    }

    // Clear response structure
    memset(response, 0, sizeof(launch_rsp_t));

    // Populate response fields
    response->status = result->status;
    response->reserved = 0;

    LOG_DBG("Built launch response: status=%d", response->status);
    return ERR_OK;
}

error_code_t build_query_device_response(const query_result_t* result, query_device_rsp_t* response)
{
    if (!result || !response) {
        return ERR_INVALID_PARAM;
    }

    // Clear response structure
    memset(response, 0, sizeof(query_device_rsp_t));

    // Populate response fields
    response->status = result->status;
    
    if (result->status == ERR_OK) {
        // Copy device info
        memcpy(&response->device_info, &result->device_info, sizeof(device_info_t));
    }

    LOG_DBG("Built query device response: status=%d", response->status);
    return ERR_OK;
}
