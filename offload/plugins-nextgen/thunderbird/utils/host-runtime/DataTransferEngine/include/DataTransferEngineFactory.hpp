/**
 * @file DataTransferEngineFactory.hpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#pragma once
#include "DataTransferBackend.hpp"
#include "DataTransferEngineWriteBase.hpp"
#include "DataTransferEngineReadBase.hpp"
#include "XILINX_XDMA_DEVICES.hpp"
#include <memory>
#include <string>

class DataTransferEngineFactory
{
public:
    /// Create a writer channel for the selected backend.
    static std::unique_ptr<DataTransferEngineWriteBase>
    createWriteChannel(DataTransferBackend backend,
                       const std::string& devNode);

    /// Create a reader channel for the selected backend.
    static std::unique_ptr<DataTransferEngineReadBase>
    createReadChannel(DataTransferBackend backend,
                      const std::string& devNode);

    // Disallow construction/destruction.
    DataTransferEngineFactory()            = delete;
    ~DataTransferEngineFactory()           = delete;
    DataTransferEngineFactory(const DataTransferEngineFactory&) = delete;
    DataTransferEngineFactory& operator=(const DataTransferEngineFactory&) = delete;
};

