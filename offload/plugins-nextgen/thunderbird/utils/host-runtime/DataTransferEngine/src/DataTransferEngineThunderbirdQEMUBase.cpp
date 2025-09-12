/**
 * @file DataTransferEngineThunderbirdQEMUBase.cpp
 * @brief Implementation of base class for Thunderbird QEMU transfer engines
 * @details Implements shared resource management and common functionality for QEMU-based
 *          data transfer engines that simulate hardware transfers through shared memory files.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-07-27
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */

#include "DataTransferEngineThunderbirdQEMUBase.hpp"
#include <filesystem>

// Static member definitions for shared resource management
std::unordered_map<std::string, std::shared_ptr<SharedDeviceResources>> ThunderbirdQEMUEngineBase::deviceResources_;
std::mutex ThunderbirdQEMUEngineBase::resourceMapMutex_;

ThunderbirdQEMUEngineBase::ThunderbirdQEMUEngineBase(std::string devNode) 
    : devNode_(std::move(devNode)) {
    // Get or create shared resources for this device node
    resources_ = getSharedResources(devNode_);
    // Initialize the shared memory file if not already done
    initializeFile();
}

std::shared_ptr<SharedDeviceResources> ThunderbirdQEMUEngineBase::getSharedResources(const std::string& devNode) {
    std::lock_guard<std::mutex> lock(resourceMapMutex_);
    
    // Check if resources already exist for this device node
    auto it = deviceResources_.find(devNode);
    if (it != deviceResources_.end()) {
        return it->second;
    }
    
    // Create new shared resources for this device node
    auto resources = std::make_shared<SharedDeviceResources>();
    deviceResources_[devNode] = resources;
    return resources;
}

std::unique_lock<std::mutex> ThunderbirdQEMUEngineBase::getScopedLock() const {
    return std::unique_lock<std::mutex>(resources_->accessMutex);
}

std::size_t ThunderbirdQEMUEngineBase::getFileSize() const {
    // File size is immutable after initialization, no lock needed
    return resources_->fileSize;
}

void ThunderbirdQEMUEngineBase::initializeFile() {
    std::lock_guard<std::mutex> lock(resources_->accessMutex);
    
    // Only initialize once per shared resource - check if already open
    if (resources_->sharedMemFile.is_open()) {
        return;
    }
    
    // Verify the QEMU shared memory file exists
    if (!std::filesystem::exists(devNode_)) {
        throw std::runtime_error("Shared memory file does not exist: " + devNode_);
    }
    
    // Get file size using modern filesystem API
    resources_->fileSize = std::filesystem::file_size(devNode_);
    
    // Open the shared memory file for read/write in binary mode
    resources_->sharedMemFile.open(devNode_, std::ios::in | std::ios::out | std::ios::binary);
    if (!resources_->sharedMemFile) {
        throw std::runtime_error("Failed to open shared memory file: " + devNode_);
    }
}

void ThunderbirdQEMUEngineBase::validateBounds(uint64_t deviceAddr, std::size_t len, 
                                               const std::string& operation_name) const {
    // Zero-length operations are always valid (no-op)
    if (len == 0) {
        return;
    }
    
    // Check if operation would exceed the shared memory file boundaries
    if (deviceAddr >= resources_->fileSize || deviceAddr + len > resources_->fileSize) {
        throw std::runtime_error("Operation exceeds file bounds");
    }
}

void ThunderbirdQEMUEngineBase::safeSeek(uint64_t pos, bool for_writing) const {
    // Seek to the specified position in read or write mode
    if (for_writing) {
        resources_->sharedMemFile.seekp(pos);
    } else {
        resources_->sharedMemFile.seekg(pos);
    }
    
    // Verify the seek operation succeeded
    if (resources_->sharedMemFile.fail()) {
        throw std::runtime_error("Failed to seek to position: " + std::to_string(pos));
    }
}

char* ThunderbirdQEMUEngineBase::ensureBufferCapacity(std::size_t required_size) const {
    // Grow buffer if needed (never shrink to avoid frequent reallocations)
    if (resources_->transferBuffer.size() < required_size) {
        resources_->transferBuffer.resize(required_size);
    }
    return resources_->transferBuffer.data();
}
