#pragma once

#include "ThunderbirdRuntime.hpp"
#include "BatchUtils.hpp"
#include "MessageUtils.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

// Buffer management utilities for testing

namespace BufferUtils {

/**
 * @brief Structure to track allocated memory buffers
 */
struct AllocatedBuffer {
    uint64_t address;
    size_t size;
    std::string purpose;  // "kernel", "args", etc.

    AllocatedBuffer() = default;
    AllocatedBuffer(uint64_t addr, size_t sz, const std::string& p)
        : address(addr), size(sz), purpose(p) {}
};

/**
 * @brief Allocate multiple buffers in a batch
 * @param writer Writer instance for sending commands
 * @param reader Reader instance for receiving responses
 * @param buffer_specs Vector of {size, purpose} pairs
 * @param out_allocated_buffers_map Map to store allocated buffer info (purpose -> buffer)
 * @return True on success
 */
template<typename Writer, typename Reader>
bool allocate_buffers(Writer& writer, Reader& reader,
                     const std::vector<std::pair<size_t, std::string>>& buffer_specs,
                     std::unordered_map<std::string, AllocatedBuffer>& out_allocated_buffers_map);

/**
 * @brief Free multiple buffers in a batch
 * @param writer Writer instance for sending commands
 * @param reader Reader instance for receiving responses
 * @param allocated_buffers_map Map of purpose -> buffer to free
 * @return True on success
 */
template<typename Writer, typename Reader>
bool free_buffers(Writer& writer, Reader& reader,
                 const std::unordered_map<std::string, AllocatedBuffer>& allocated_buffers_map);

/**
 * @brief Find allocated buffer by purpose
 * @param buffers Vector of allocated buffers
 * @param purpose Purpose string to search for
 * @return Pointer to buffer or nullptr if not found
 */
AllocatedBuffer* find_buffer_by_purpose(std::vector<AllocatedBuffer>& buffers, const std::string& purpose);

/**
 * @brief Find multiple required buffers by their purposes
 * @param buffers_map Map of purpose -> buffer to search in
 * @param purposes Vector of purpose strings to search for
 * @param out_found_buffers Map to store purpose -> buffer pointer mappings
 * @return True if all required buffers were found, false otherwise
 */
bool find_required_buffers(const std::unordered_map<std::string, AllocatedBuffer>& buffers_map,
                          const std::vector<std::string>& purposes,
                          std::unordered_map<std::string, AllocatedBuffer*>& out_found_buffers);

} // namespace BufferUtils

// Include template implementations
#include "BufferUtils.tpp"
