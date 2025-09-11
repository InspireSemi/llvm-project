/**
 * @file DataTransferEngineReadThunderbirdQEMU.hpp
 * @brief Read-specific implementation for Thunderbird QEMU data transfer engine
 * @details Implements read operations from QEMU shared memory files, simulating
 *          device-to-host DMA transfers for QEMU-based hardware emulation.
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */
#pragma once
#include "DataTransferEngineReadBase.hpp"
#include "DataTransferEngineThunderbirdQEMUBase.hpp"
#include <string>
#include <fstream>
#include <vector>

/**
 * @brief QEMU-based read transfer engine for simulating device-to-host DMA
 * @details Implements read operations from QEMU shared memory files, providing
 *          a software simulation of hardware DMA read transfers. Uses shared
 *          file access with thread-safe serialization across all instances.
 */
class DataTransferEngineReadThunderbirdQEMU final 
    : public DataTransferEngineReadBase, private ThunderbirdQEMUEngineBase {
public:
    /**
     * @brief Construct a new QEMU read transfer engine
     * @param devNode Path to the QEMU shared memory device node file
     */
    explicit DataTransferEngineReadThunderbirdQEMU(std::string devNode);

    /**
     * @brief Perform a blocking read transfer from device memory to host buffer
     * @param deviceAddr Device memory address (offset in shared memory file)
     * @param hostBuf Host buffer to read data into
     * @param len Number of bytes to transfer
     * @return Number of bytes actually transferred
     * @throws std::runtime_error on transfer errors or boundary violations
     * @details Simulates device-to-host DMA by reading from the QEMU shared memory file
     */
    std::size_t transfer(uint64_t deviceAddr,
                         void* hostBuf,
                         std::size_t len) override;

    /**
     * @brief Perform a blocking read transfer from device memory to host file
     * @param deviceAddr Device memory address (offset in shared memory file)
     * @param hostPath Path to the host file to write data to
     * @param len Number of bytes to transfer
     * @return Number of bytes actually transferred
     * @throws std::runtime_error on file errors or boundary violations
     * @details Simulates device-to-host DMA by reading from shared memory and writing to file
     */
    std::size_t transferFile(uint64_t deviceAddr,
                             const std::string& hostPath,
                             std::size_t len) override;
};

