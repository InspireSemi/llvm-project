/**
 * @file DataTransferEngineThunderbirdQEMUBase.hpp
 * @brief Base class for Thunderbird QEMU transfer engines with shared file access
 * @details This class provides common functionality for QEMU-based data transfer engines
 *          that simulate hardware transfers through shared memory files. All instances
 *          accessing the same device node share resources and use serialized access.
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-07-27
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */
#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <filesystem>

/**
 * @brief Shared resources for a specific device node (QEMU shared memory file)
 * @details Contains file handle, size, and transfer buffer shared among all instances
 *          accessing the same device node. Thread-safe access is enforced via mutex.
 */
struct SharedDeviceResources {
    std::fstream sharedMemFile;           ///< File stream for the QEMU shared memory file
    std::size_t fileSize;                 ///< Size of the shared memory file in bytes
    mutable std::vector<char> transferBuffer; ///< Reusable buffer to avoid frequent allocations
    mutable std::mutex accessMutex;       ///< Serializes access to all shared resources
    
    SharedDeviceResources() : fileSize(0) {}
};

/**
 * @brief Base class for Thunderbird QEMU transfer engines with shared file management
 * @details Provides common functionality for QEMU-based data transfer engines that simulate
 *          hardware transfers through shared memory files. Key features:
 *          - Thread-safe shared resource management across instances
 *          - Automatic resource cleanup using RAII
 *          - Optimized buffer reuse to minimize allocations
 *          - Serialized file access to prevent corruption
 */
class ThunderbirdQEMUEngineBase {
private:
    /// Map of device node paths to their shared resources
    static std::unordered_map<std::string, std::shared_ptr<SharedDeviceResources>> deviceResources_;
    /// Protects access to the deviceResources_ map
    static std::mutex resourceMapMutex_;
    
protected:
    std::string devNode_;                              ///< Path to the QEMU shared memory device node
    std::shared_ptr<SharedDeviceResources> resources_; ///< Shared resources for this device node
    
    /// Default size for the internal transfer buffer (8KB)
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 8192;
    
    /**
     * @brief Get or create shared resources for a device node
     * @param devNode The device node path (QEMU shared memory file)
     * @return Shared pointer to the device resources
     * @details Thread-safe method that returns existing resources for the device node
     *          or creates new ones if this is the first instance for this node.
     */
    static std::shared_ptr<SharedDeviceResources> getSharedResources(const std::string& devNode);
    
    /**
     * @brief Initialize the shared memory file and determine its size
     * @throws std::runtime_error if file cannot be opened or sized
     * @details Opens the QEMU shared memory file and determines its size using filesystem APIs.
     *          Only performs initialization once per shared resource instance.
     */
    void initializeFile();
    
    /**
     * @brief Validate device address and length against file boundaries
     * @param deviceAddr The device address to validate (offset in shared memory file)
     * @param len The length of the operation in bytes
     * @param operation_name Name of the operation for error messages
     * @throws std::runtime_error if validation fails
     * @details Checks that the operation fits within the shared memory file boundaries.
     *          Zero-length operations are always considered valid.
     * @note This method assumes the access mutex is already held by the caller
     */
    void validateBounds(uint64_t deviceAddr, std::size_t len, 
                       const std::string& operation_name = "operation") const;
    
    /**
     * @brief Safe seek operation with error handling
     * @param pos The position to seek to in the shared memory file
     * @param for_writing Whether this is for writing (true) or reading (false)
     * @throws std::runtime_error if seek fails
     * @details Seeks to the specified position in the shared memory file and
     *          verifies the operation succeeded.
     * @note This method assumes the access mutex is already held by the caller
     */
    void safeSeek(uint64_t pos, bool for_writing = false) const;
    
    /**
     * @brief Ensure buffer has at least the requested capacity
     * @param required_size The minimum size needed in bytes
     * @return Pointer to the buffer data
     * @throws std::runtime_error if resize fails
     * @details Grows the internal transfer buffer if needed to accommodate the requested size.
     *          The buffer never shrinks to avoid frequent reallocations.
     * @note This method assumes the access mutex is already held by the caller
     */
    char* ensureBufferCapacity(std::size_t required_size) const;
    
public:
    /**
     * @brief Construct a new Thunderbird QEMU Engine Base object
     * @param devNode Path to the QEMU shared memory device node file
     * @details Initializes the base engine with the specified device node,
     *          sets up shared resources, and opens the shared memory file.
     */
    explicit ThunderbirdQEMUEngineBase(std::string devNode);
    
    /**
     * @brief Destroy the Thunderbird QEMU Engine Base object
     * @details Uses default destructor - cleanup is handled automatically via RAII
     */
    virtual ~ThunderbirdQEMUEngineBase() = default;
    
    /**
     * @brief Get a scoped lock for thread-safe access to shared resources
     * @return A unique_lock that guards access to the shared resources
     * @details Returns a RAII lock object that automatically releases when it goes out of scope.
     *          This ensures thread-safe access to the shared memory file and buffer.
     */
    std::unique_lock<std::mutex> getScopedLock() const;
    
    /**
     * @brief Get the file size (thread-safe)
     * @return The size of the shared memory file in bytes
     * @details Returns the size of the QEMU shared memory file. This value is determined
     *          during initialization and is immutable thereafter.
     */
    std::size_t getFileSize() const;
};
