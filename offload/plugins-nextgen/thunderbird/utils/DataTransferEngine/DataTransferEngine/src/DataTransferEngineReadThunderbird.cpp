/**
 * @file DataTransferEngineReadThunderbird.cpp
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi InspireSemi
 * 
 */
#include "DataTransferEngineReadThunderbird.hpp"
#include <stdexcept>

DataTransferEngineReadThunderbird::DataTransferEngineReadThunderbird(std::string /*node*/) {}

std::size_t DataTransferEngineReadThunderbird::transfer(
        uint64_t, void*, std::size_t)
{
    throw std::runtime_error("Thunderbird read channel not implemented yet");
}

std::size_t DataTransferEngineReadThunderbird::transferFile(
        uint64_t, const std::string&, std::size_t)
{
    throw std::runtime_error("Thunderbird read channel not implemented yet");
}

