/**
 * @file ElfParser.hpp
 * @author Michael Brothers
 * @brief ELF file parser for finding symbol offsets (simplified for PIC flat binary entry points)
 */

#pragma once

#include <string>
#include <cstdint>
#include <vector>

/**
 * @brief Information extracted from ELF file analysis (simplified)
 */
class ElfInfo {
public:
    uint64_t entry_binary_offset = 0;   ///< Offset of the entry symbol in the flat binary
    bool found_entry = false;           ///< Whether entry symbol was found
    std::vector<uint8_t> entry_bytes;   ///< First 16 bytes at the entry point for verification
};

/**
 * @brief ELF file parser for finding symbol offsets (PIC-only, simplified)
 */
class ElfParser {
public:
    /**
     * @brief Process ELF file and flat binary to extract symbol information
     * @param elf_path Path to the ELF file
     * @param flat_binary The flat binary data
     * @param entry_symbol Name of the entry symbol to locate
     * @param info Reference to ElfInfo object to populate with results
     * @return 0 on success, -1 on failure
     */
    static int determineBinaryEntryOffset(const std::string& elf_path,
                                   const std::vector<uint8_t>& flat_binary,
                                   const std::string& entry_symbol,
                                   ElfInfo& info);

private:
    /**
     * @brief Find a symbol in an ELF file and extract bytes from its location
     * @param elf_path Path to the ELF file
     * @param symbol_name Name of the symbol to find
     * @param num_bytes Number of bytes to copy from the symbol location
     * @param symbol_bytes Output vector to store the extracted bytes
     * @return 0 on success, -1 on failure
     */
    static int findSymbolAndExtractBytes(const std::string& elf_path,
                                        const std::string& symbol_name,
                                        size_t num_bytes,
                                        std::vector<uint8_t>& symbol_bytes);
};