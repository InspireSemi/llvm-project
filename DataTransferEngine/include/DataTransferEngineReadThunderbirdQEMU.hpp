/**
 * @file DataTransferEngineReadThunderbirdQEMU.hpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#pragma once
#include "DataTransferEngineReadBase.hpp"
#include <string>

class DataTransferEngineReadThunderbirdQEMU final : public DataTransferEngineReadBase {
public:
    explicit DataTransferEngineReadThunderbirdQEMU(std::string devNode);

    /* Blocking DMA transfer (device → host) – not yet implemented */
    std::size_t transfer(uint64_t deviceAddr,
                         void* hostBuf,
                         std::size_t len) override;

    /* DMA into a host-side file – not yet implemented */
    std::size_t transferFile(uint64_t deviceAddr,
                             const std::string& hostPath,
                             std::size_t len) override;

private:
    std::string devNode_;
};

