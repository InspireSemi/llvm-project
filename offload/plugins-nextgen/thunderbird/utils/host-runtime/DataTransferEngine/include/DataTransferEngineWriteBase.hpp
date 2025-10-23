/**
 * @file DataTransferEngineWriteBase.hpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

class DataTransferEngineWriteBase {
public:
    virtual ~DataTransferEngineWriteBase() = default;

    virtual std::size_t transfer(
        uint64_t deviceAddr,
        const void* hostBuf,
        std::size_t len) = 0;

    virtual std::size_t transferFile(
        uint64_t deviceAddr,
        const std::string& hostPath,
        std::size_t len) = 0;
};

