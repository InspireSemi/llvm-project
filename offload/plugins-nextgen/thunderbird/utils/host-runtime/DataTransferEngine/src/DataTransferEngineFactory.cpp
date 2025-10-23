/**
 * @file DataTransferEngineFactory.cpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#include "DataTransferEngineFactory.hpp"
#include "DataTransferEngineWriteXilinx.hpp"
#include "DataTransferEngineReadXilinx.hpp"
#include "DataTransferEngineWriteThunderbird.hpp"
#include "DataTransferEngineReadThunderbird.hpp"
#include "DataTransferEngineWriteThunderbirdQEMU.hpp"
#include "DataTransferEngineReadThunderbirdQEMU.hpp"

std::unique_ptr<DataTransferEngineWriteBase>
DataTransferEngineFactory::createWriteChannel(DataTransferBackend backend,
                                              const std::string& node)
{
    switch (backend)
    {
        case DataTransferBackend::Xilinx:          return std::make_unique<DataTransferEngineWriteXilinx>(node);
        case DataTransferBackend::Thunderbird:     return std::make_unique<DataTransferEngineWriteThunderbird>(node);
        case DataTransferBackend::ThunderbirdQEMU: return std::make_unique<DataTransferEngineWriteThunderbirdQEMU>(node);
        default:                   return nullptr;
    }
}

std::unique_ptr<DataTransferEngineReadBase>
DataTransferEngineFactory::createReadChannel(DataTransferBackend backend,
                                             const std::string& node)
{
    switch (backend)
    {
        case DataTransferBackend::Xilinx:          return std::make_unique<DataTransferEngineReadXilinx>(node);
        case DataTransferBackend::Thunderbird:     return std::make_unique<DataTransferEngineReadThunderbird>(node);
        case DataTransferBackend::ThunderbirdQEMU: return std::make_unique<DataTransferEngineReadThunderbirdQEMU>(node);
        default:                   return nullptr;
    }
}

