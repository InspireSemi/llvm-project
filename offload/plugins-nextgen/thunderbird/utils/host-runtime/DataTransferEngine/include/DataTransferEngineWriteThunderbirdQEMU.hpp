/**
 * @file DataTransferEngineWriteThunderbirdQEMU.hpp
 * @brief Write-specific implementation for Thunderbird QEMU data transfer engine
 * @details Implements write operations to QEMU shared memory files, simulating
 *          host-to-device DMA transfers for QEMU-based hardware emulation.
 * @author Jacob Longwell (jlongwell@inspiresemi.com)
 * @author Michael Brothers (mBrothers@inspiresemi.com)
 * @version 0.1
 * @date 2025-07-15
 * 
 * @copyright Copyright (c) 2025 InspireSemi
 */
#pragma once
#include "DataTransferEngineWriteBase.hpp"
#include "DataTransferEngineThunderbirdQEMUBase.hpp"
#include <string>
#include <fstream>
#include <vector>

/**
 * @brief QEMU-based write transfer engine for simulating host-to-device DMA
 * @details Implements write operations to QEMU shared memory files, providing
 *          a software simulation of hardware DMA write transfers. Uses shared
 *          file access with thread-safe serialization across all instances.
 */
class DataTransferEngineWriteThunderbirdQEMU final 
    : public DataTransferEngineWriteBase, private ThunderbirdQEMUEngineBase {
public:
    /**
     * @brief Construct a new QEMU write transfer engine
     * @param devNode Path to the QEMU shared memory device node file
     */
    explicit DataTransferEngineWriteThunderbirdQEMU(std::string devNode);

    /**
     * @brief Perform a blocking write transfer from host buffer to device memory
     * @param deviceAddr Device memory address (offset in shared memory file)
     * @param hostBuf Host buffer containing data to write
     * @param len Number of bytes to transfer
     * @return Number of bytes actually transferred
     * @throws std::runtime_error on transfer errors or boundary violations
     * @details Simulates host-to-device DMA by writing to the QEMU shared memory file
     */
    std::size_t transfer(uint64_t deviceAddr,
                         const void* hostBuf,
                         std::size_t len) override;

    /**
     * @brief Perform a blocking write transfer from host file to device memory
     * @param deviceAddr Device memory address (offset in shared memory file)
     * @param hostPath Path to the host file to read data from
     * @param len Number of bytes to transfer
     * @return Number of bytes actually transferred
     * @throws std::runtime_error on file errors or boundary violations
     * @details Simulates host-to-device DMA by reading from file and writing to shared memory
     */
    std::size_t transferFile(uint64_t deviceAddr,
                             const std::string& hostPath,
                             std::size_t len) override;
};

