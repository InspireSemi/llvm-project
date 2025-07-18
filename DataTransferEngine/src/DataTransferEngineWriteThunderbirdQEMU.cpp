/**
 * @file DataTransferEngineWriteThunderbirdQEMU.cpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#include "DataTransferEngineWriteThunderbirdQEMU.hpp"
#include <stdexcept>

DataTransferEngineWriteThunderbirdQEMU::DataTransferEngineWriteThunderbirdQEMU(std::string /*node*/) {}

std::size_t DataTransferEngineWriteThunderbirdQEMU::transfer(
        uint64_t, const void*, std::size_t)
{
    throw std::runtime_error("ThunderbirdQEMU write channel not implemented yet");
}

std::size_t DataTransferEngineWriteThunderbirdQEMU::transferFile(
        uint64_t, const std::string&, std::size_t)
{
    throw std::runtime_error("ThunderbirdQEMU write channel not implemented yet");
}
