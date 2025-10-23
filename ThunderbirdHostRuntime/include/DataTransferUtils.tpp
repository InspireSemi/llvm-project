/**
 * @brief Write a vector of floats to device memory
 * @param writer Writer instance
 * @param allocated_address Device address to write to
 * @param data Vector of floats to write
 * @param name Descriptive name for logging
 * @return True on success
 */
template<typename Writer>
bool write_vector_to_device(Writer& writer, uint64_t allocated_address, const std::vector<float>& data, const std::string& name) {
    std::cout << "Writing " << name << " vector to allocated memory..." << std::endl;

    size_t byte_size = data.size() * sizeof(float);
    int64_t written = writer.transfer(allocated_address - IVSHMEM_SHM_BASE,
                                     reinterpret_cast<const uint8_t*>(data.data()),
                                     byte_size);
    if (written != static_cast<int64_t>(byte_size)) {
        std::cerr << "Error: Failed to write " << name << " vector to device memory (written=" << written
                  << ", expected=" << byte_size << ")" << std::endl;
        return false;
    }

    std::cout << "✓ Successfully wrote " << data.size() << " floats (" << byte_size
              << " bytes) for " << name << " vector to device memory" << std::endl;
    return true;
}

/**
 * @brief Read a vector of floats from device memory
 * @param reader Reader instance
 * @param allocated_address Device address to read from
 * @param data Vector to store read floats
 * @param name Descriptive name for logging
 * @return True on success
 */
template<typename Reader>
bool read_vector_from_device(Reader& reader, uint64_t allocated_address, std::vector<float>& data, const std::string& name) {
    std::cout << "Reading " << name << " vector from device memory for verification..." << std::endl;

    size_t byte_size = data.size() * sizeof(float);
    int64_t bytes_read = reader.transfer(allocated_address - IVSHMEM_SHM_BASE,
                                         reinterpret_cast<uint8_t*>(data.data()),
                                         byte_size);

    if (bytes_read != static_cast<int64_t>(byte_size)) {
        std::cerr << "Error: Failed to read " << name << " vector from device memory (read="
                  << bytes_read << ", expected=" << byte_size << ")" << std::endl;
        return false;
    }

    std::cout << "✓ Successfully read " << data.size() << " floats (" << byte_size
              << " bytes) for " << name << " vector from device memory" << std::endl;
    return true;
}

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
                                      const std::string& name) {
    std::vector<float> readback_data(original_data.size());

    if (!read_vector_from_device(reader, allocated_address, readback_data, name)) {
        return false;
    }

    const float tolerance = 1e-6f;
    for (size_t i = 0; i < original_data.size(); ++i) {
        if (std::abs(readback_data[i] - original_data[i]) > tolerance) {
            std::cerr << name << " vector mismatch at index " << i << ": wrote=" << original_data[i]
                      << ", read=" << readback_data[i] << std::endl;
            return false;
        }
    }

    std::cout << "✓ " << name << " vector data transmission verification successful" << std::endl;
    return true;
}

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
bool write_data_to_device(Writer& writer, uint64_t allocated_address, const void* data, size_t size, const std::string& description) {
    std::cout << "Writing " << description << " to allocated memory..." << std::endl;

    if (!data) {
        std::cerr << "Error: data pointer is null" << std::endl;
        return false;
    }

    int64_t written = writer.transfer(allocated_address - IVSHMEM_SHM_BASE,
                                     reinterpret_cast<const uint8_t*>(data),
                                     size);
    if (written != static_cast<int64_t>(size)) {
        std::cerr << "Error: Failed to write " << description << " to device memory (written=" << written
                  << ", expected=" << size << ")" << std::endl;
        return false;
    }

    std::cout << "✓ Successfully wrote " << size << " bytes of " << description << " to device memory" << std::endl;
    return true;
}