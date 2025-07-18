/**
 * @file DataTransferEngineWriteThunderbirdQEMU.hpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#pragma once
#include "DataTransferEngineWriteBase.hpp"
#include <string>

class DataTransferEngineWriteThunderbirdQEMU final : public DataTransferEngineWriteBase {
public:
    explicit DataTransferEngineWriteThunderbirdQEMU(std::string devNode);

    /* Blocking DMA transfer (to be implemented) */
    std::size_t transfer(uint64_t deviceAddr,
                         const void* hostBuf,
                         std::size_t len) override;

    /* Transfer entire host file into device memory (to be implemented) */
    std::size_t transferFile(uint64_t deviceAddr,
                             const std::string& hostPath,
                             std::size_t len) override;

private:
    std::string devNode_;
};

