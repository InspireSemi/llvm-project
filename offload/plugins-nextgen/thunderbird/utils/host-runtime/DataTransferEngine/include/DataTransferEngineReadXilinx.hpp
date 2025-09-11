/**
 * @file DataTransferEngineReadXilinx.hpp
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

class DataTransferEngineReadXilinx final : public DataTransferEngineReadBase {
public:
    explicit DataTransferEngineReadXilinx(std::string devNode);
    ~DataTransferEngineReadXilinx() override;

    /* Blocking DMA transfer into caller-provided buffer */
    std::size_t transfer(uint64_t deviceAddr,
                         void* hostBuf,
                         std::size_t len) override;

    /* Transfer straight into a host file */
    std::size_t transferFile(uint64_t deviceAddr,
                             const std::string& hostPath,
                             std::size_t len) override;

private:
    void openIfNeeded();

    int         fd_{-1};
    std::string devNode_;
};
