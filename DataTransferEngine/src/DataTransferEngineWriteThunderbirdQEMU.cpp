/**
 * @file DataTransferEngineWriteThunderbirdQEMU.cpp
 * @brief Implementation of QEMU-based write data transfer engine
 * @details Provides write operations to QEMU shared memory files, simulating
 *          host-to-device DMA transfers for hardware emulation and testing.
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */
#include "DataTransferEngineWriteThunderbirdQEMU.hpp"
#include <stdexcept>
#include <fstream>
#include <vector>

DataTransferEngineWriteThunderbirdQEMU::DataTransferEngineWriteThunderbirdQEMU(std::string devNode)
    : ThunderbirdQEMUEngineBase(std::move(devNode)) {
    // Constructor delegates to base class for shared resource setup
}

std::size_t DataTransferEngineWriteThunderbirdQEMU::transfer(
        uint64_t deviceAddr, const void* hostBuf, std::size_t len)
{
    // Validate input parameters
    if (!hostBuf) {
        throw std::runtime_error("Host buffer is null");
    }
    
    // Get scoped lock for thread-safe access to shared resources
    auto lock = getScopedLock();
    
    // Validate that operation fits within shared memory file boundaries
    validateBounds(deviceAddr, len, "Write operation");

    // Validate the address regardless but now bail if len is zero
    if (len == 0) {
        return 0; // No data to write, just validate bounds
    }

    // Seek to the specified device address in the shared memory file
    safeSeek(deviceAddr, true);
    
    // Write data directly from host buffer to shared memory file
    resources_->sharedMemFile.write(static_cast<const char*>(hostBuf), len);
    if (resources_->sharedMemFile.fail()) {
        throw std::runtime_error("Failed to write data to shared memory file");
    }
    
    // Flush data to ensure it's written to the file system
    resources_->sharedMemFile.flush();
    if (resources_->sharedMemFile.fail()) {
        throw std::runtime_error("Failed to flush data to shared memory");
    }
    
    return len;
}

std::size_t DataTransferEngineWriteThunderbirdQEMU::transferFile(
        uint64_t deviceAddr, const std::string& hostPath, std::size_t len)
{
    // Get scoped lock for thread-safe access to shared resources
    auto lock = getScopedLock();
    
    // Validate that operation fits within shared memory file boundaries
    validateBounds(deviceAddr, len, "Write file operation");

    // Validate the address regardless but now bail if len is zero
    if (len == 0) {
        return 0; // No data to write, just validate bounds
    }
    
    // Open the input file for reading in binary mode
    std::ifstream src(hostPath, std::ios::binary);
    if (!src.is_open()) {
        throw std::runtime_error("Failed to open input file: " + hostPath);
    }
    
    // Validate file size - ensure it has at least 'len' bytes
    src.seekg(0, std::ios::end);
    std::size_t file_size = src.tellg();
    if (file_size < len) {
        throw std::runtime_error("Input file is too small: requires " + std::to_string(len) + 
                                 " bytes, but file only has " + std::to_string(file_size) + " bytes");
    }
    src.seekg(0, std::ios::beg);  // Reset to beginning
    
    // Seek to the specified device address in the shared memory file
    safeSeek(deviceAddr, true);
    
    // Use the optimized transfer buffer to avoid frequent allocations
    char* buffer = ensureBufferCapacity(len);
    
    // Read data from input file into the transfer buffer
    src.read(buffer, len);
    std::size_t bytes_read = src.gcount();
    
    // Check for read errors - require complete read
    if (bytes_read != len) {
        throw std::runtime_error("Failed to read complete requested amount of data from input file");
    }
    
    // Write the data to shared memory file
    resources_->sharedMemFile.write(buffer, bytes_read);
    if (resources_->sharedMemFile.fail()) {
        throw std::runtime_error("Failed to write to shared memory file");
    }
    // Flush data to ensure it's written to the file system
    resources_->sharedMemFile.flush();
    if (resources_->sharedMemFile.fail()) {
        throw std::runtime_error("Failed to flush data to shared memory");
    }
    
    return bytes_read;
}
