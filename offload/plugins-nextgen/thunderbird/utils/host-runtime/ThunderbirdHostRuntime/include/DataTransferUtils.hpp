#pragma once

#include "ThunderbirdRuntime.hpp"
#include "ivshmem_abi.h"

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <random>

// Data transfer and verification utilities for testing

namespace DataTransferUtils {

//constexpr uint64_t IVSHMEM_SHM_BASE = 0x82000000000000ull;

/**
 * @brief Generate a random vector of floats
 * @param size Number of elements
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @return Vector of random floats
 */
std::vector<float> generate_random_vector(size_t size, float min_val, float max_val);

/**
 * @brief Write a vector of floats to device memory
 * @param writer Writer instance
 * @param allocated_address Device address to write to
 * @param data Vector of floats to write
 * @param name Descriptive name for logging
 * @return True on success
 */
template<typename Writer>
bool write_vector_to_device(Writer& writer, uint64_t allocated_address, const std::vector<float>& data, const std::string& name);

/**
 * @brief Read a vector of floats from device memory
 * @param reader Reader instance
 * @param allocated_address Device address to read from
 * @param data Vector to store read floats
 * @param name Descriptive name for logging
 * @return True on success
 */
template<typename Reader>
bool read_vector_from_device(Reader& reader, uint64_t allocated_address, std::vector<float>& data, const std::string& name);

/**
 * @brief Verify vector transmission by reading back and comparing
 * @param reader Reader instance
 * @param allocated_address Device address
 * @param original_data Original data sent
 * @param name Descriptive name for logging
 * @return True if data matches
 */
template<typename Reader>
bool verify_vector_transmission(Reader& reader,
                                      uint64_t allocated_address,
                                      const std::vector<float>& original_data,
                                      const std::string& name);

/**
 * @brief Write raw data to device memory
 * @param writer Writer instance
 * @param allocated_address Device address to write to
 * @param data Pointer to data to write
 * @param size Size of data in bytes
 * @param description Descriptive name for logging
 * @return True on success
 */
template<typename Writer>
bool write_data_to_device(Writer& writer, uint64_t allocated_address, const void* data, size_t size, const std::string& description);

#include "DataTransferUtils.tpp"

} // namespace DataTransferUtils