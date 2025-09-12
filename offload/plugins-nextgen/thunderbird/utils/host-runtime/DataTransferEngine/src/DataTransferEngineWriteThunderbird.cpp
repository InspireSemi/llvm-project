/**
 * @file DataTransferEngineWriteThunderbird.cpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 * 
 */
#include "DataTransferEngineWriteThunderbird.hpp"
#include <stdexcept>

DataTransferEngineWriteThunderbird::DataTransferEngineWriteThunderbird(std::string /*node*/) {}

std::size_t DataTransferEngineWriteThunderbird::transfer(
        uint64_t, const void*, std::size_t)
{
    throw std::runtime_error("Thunderbird write channel not implemented yet");
}

std::size_t DataTransferEngineWriteThunderbird::transferFile(
        uint64_t, const std::string&, std::size_t)
{
    throw std::runtime_error("Thunderbird write channel not implemented yet");
}

