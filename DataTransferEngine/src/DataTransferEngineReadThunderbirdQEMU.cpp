/**
 * @file DataTransferEngineReadThunderbirdQEMU.cpp
 * @brief Implementation of QEMU-based read data transfer engine
 * @details Provides read operations from QEMU shared memory files, simulating
 *          device-to-host DMA transfers for hardware emulation and testing.
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */
#include "DataTransferEngineReadThunderbirdQEMU.hpp"
#include <stdexcept>
#include <fstream>
#include <vector>

DataTransferEngineReadThunderbirdQEMU::DataTransferEngineReadThunderbirdQEMU(std::string devNode) 
    : ThunderbirdQEMUEngineBase(std::move(devNode)) {
    // Constructor delegates to base class for shared resource setup
}

std::size_t DataTransferEngineReadThunderbirdQEMU::transfer(
        uint64_t deviceAddr, void* hostBuf, std::size_t len)
{
    // Validate input parameters
    if (!hostBuf) {
        throw std::runtime_error("Host buffer is null");
    }
    
    // Get scoped lock for thread-safe access to shared resources
    auto lock = getScopedLock();
    
    // Validate that operation fits within shared memory file boundaries
    validateBounds(deviceAddr, len, "Read operation");

    // Validate the address regardless but now bail if len is zero
    if (len == 0) {
        return 0; // No data to read, just validate bounds
    }
    
    // Seek to the specified device address in the shared memory file
    safeSeek(deviceAddr, false);
    
    // Read data directly from shared memory file into host buffer
    resources_->sharedMemFile.read(static_cast<char*>(hostBuf), len);
    std::size_t bytes_read = resources_->sharedMemFile.gcount();
    
    // Check for read errors - require complete read
    if (bytes_read != len) {
        throw std::runtime_error("Failed to read complete requested amount of data");
    }
    
    return bytes_read;
}

std::size_t DataTransferEngineReadThunderbirdQEMU::transferFile(
        uint64_t deviceAddr, const std::string& hostPath, std::size_t len)
{
    // Get scoped lock for thread-safe access to shared resources
    auto lock = getScopedLock();
    
    // Validate that operation fits within shared memory file boundaries
    validateBounds(deviceAddr, len, "Read file operation");

    // Validate the address regardless but now bail if len is zero
    if (len == 0) {
        return 0; // No data to read, just validate bounds
    }
    
    // Seek to the specified device address in the shared memory file
    safeSeek(deviceAddr, false);
    
    // Open the output file for writing in binary mode
    std::ofstream dst(hostPath, std::ios::binary);
    if (!dst.is_open()) {
        throw std::runtime_error("Failed to open output file: " + hostPath);
    }
    
    // Use the optimized transfer buffer to avoid frequent allocations
    char* buffer = ensureBufferCapacity(len);
    
    // Read data from shared memory file into the transfer buffer
    resources_->sharedMemFile.read(buffer, len);
    std::size_t bytes_read = resources_->sharedMemFile.gcount();
    
    // Write the data to the output file if any was read
    if (bytes_read > 0) {
        dst.write(buffer, bytes_read);
        if (dst.fail()) {
            throw std::runtime_error("Failed to write to output file: " + hostPath);
        }
    }
    
    // Check for read errors - require complete read
    if (bytes_read != len) {
        throw std::runtime_error("Failed to read complete requested amount of data");
    }
    
    return bytes_read;
}
