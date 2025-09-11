#include "memory_response_builder.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(memory_response_builder, LOG_LEVEL_DBG);

error_code_t build_malloc_response(const malloc_result_t* result, malloc_rsp_t* response)
{
    if (!result || !response) {
        return ERR_INVALID_PARAM;
    }

    // Clear response structure
    memset(response, 0, sizeof(malloc_rsp_t));

    // Populate response fields
    response->status = result->status;
    response->address = result->address;
    response->reserved = 0;

    LOG_DBG("Built malloc response: status=%d, address=0x%llx", response->status, response->address);
    return ERR_OK;
}

error_code_t build_free_response(error_code_t free_status, free_rsp_t* response)
{
    if (!response) {
        return ERR_INVALID_PARAM;
    }

    // Clear response structure
    memset(response, 0, sizeof(free_rsp_t));

    // Populate response fields
    response->status = free_status;
    response->reserved = 0;

    LOG_DBG("Built free response: status=%d", response->status);
    return ERR_OK;
}
