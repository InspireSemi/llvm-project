#include "ElfParser.hpp"
#include <iostream>
#include <cstring>
#include <libelf.h>
#include <gelf.h>
#include <vector>
#include <fstream>
#include <algorithm>  // for std::sort and std::search

int ElfParser::findSymbolAndExtractBytes(const std::string& elf_path,
                                        const std::string& symbol_name,
                                        size_t num_bytes,
                                        std::vector<uint8_t>& symbol_bytes) {
    // Load ELF file into memory
    std::ifstream elf_file(elf_path, std::ios::binary | std::ios::ate);
    if (!elf_file) {
        std::cerr << "Error: Failed to open ELF file " << elf_path << std::endl;
        return -1;
    }
    
    std::streamsize elf_size = elf_file.tellg();
    if (elf_size <= 0) {
        std::cerr << "Error: ELF file " << elf_path << " is empty" << std::endl;
        return -1;
    }
    
    elf_file.seekg(0, std::ios::beg);
    std::vector<uint8_t> elf_data(static_cast<size_t>(elf_size));
    if (!elf_file.read(reinterpret_cast<char*>(elf_data.data()), elf_size)) {
        std::cerr << "Error: Failed to read ELF file " << elf_path << std::endl;
        return -1;
    }

    // Initialize libelf
    if (elf_version(EV_CURRENT) == EV_NONE) {
        std::cerr << "libelf initialization failed" << std::endl;
        return -1;
    }
    
    // Create ELF from memory
    Elf* elf = elf_memory(const_cast<char*>(reinterpret_cast<const char*>(elf_data.data())), elf_data.size());
    if (!elf) {
        std::cerr << "elf_memory failed: " << elf_errmsg(-1) << std::endl;
        return -1;
    }
    
    // Clear output parameter
    symbol_bytes.clear();
    
    // Find the symbol in the symbol table
    Elf_Scn* scn = nullptr;
    GElf_Shdr shdr;
    bool symbol_found = false;
    
    while ((scn = elf_nextscn(elf, scn)) != nullptr) {
        if (gelf_getshdr(scn, &shdr) != &shdr) continue;
        
        if (shdr.sh_type == SHT_SYMTAB) {
            Elf_Data* data = elf_getdata(scn, nullptr);
            if (!data) continue;
            
            size_t nsyms = data->d_size / shdr.sh_entsize;
            for (size_t i = 0; i < nsyms; i++) {
                GElf_Sym sym;
                if (gelf_getsym(data, i, &sym) != &sym) continue;
                
                char* sym_name = elf_strptr(elf, shdr.sh_link, sym.st_name);
                if (sym_name && strcmp(sym_name, symbol_name.c_str()) == 0) {
                    // Get the section containing this symbol
                    Elf_Scn* sym_scn = elf_getscn(elf, sym.st_shndx);
                    if (sym_scn) {
                        GElf_Shdr sym_shdr;
                        if (gelf_getshdr(sym_scn, &sym_shdr) == &sym_shdr) {
                            // Calculate the symbol's file offset
                            uint64_t symbol_file_offset = sym_shdr.sh_offset + sym.st_value;
                            
                            // Ensure we don't read beyond the section boundary
                            uint64_t section_end = sym_shdr.sh_offset + sym_shdr.sh_size;
                            uint64_t max_bytes_available = section_end - symbol_file_offset;
                            size_t bytes_to_copy = std::min(num_bytes, static_cast<size_t>(max_bytes_available));
                            
                            // Ensure we don't read beyond the file boundary
                            if (symbol_file_offset + bytes_to_copy <= elf_data.size()) {
                                symbol_bytes.assign(elf_data.begin() + symbol_file_offset, 
                                                   elf_data.begin() + symbol_file_offset + bytes_to_copy);
                                symbol_found = true;
                                break;
                            } else {
                                std::cerr << "Error: Symbol bytes would exceed file bounds" << std::endl;
                                elf_end(elf);
                                return -1;
                            }
                        }
                    }
                }
            }
        }
        
        if (symbol_found) break;
    }
    
    elf_end(elf);
    
    if (!symbol_found) {
        std::cerr << "Symbol '" << symbol_name << "' not found" << std::endl;
        return -1;
    }
    
    return 0;
}

int ElfParser::determineBinaryEntryOffset(const std::string& elf_path,
                                         const std::vector<uint8_t>& flat_binary,
                                         const std::string& entry_symbol,
                                         ElfInfo& info) {
    // Clear the info object
    info = ElfInfo();
    
    // Use findSymbolAndExtractBytes to get the byte pattern from the symbol
    std::vector<uint8_t> symbol_bytes;
    const size_t BYTES_TO_EXTRACT = 16; // Extract first 16 bytes for pattern matching
    
    if (findSymbolAndExtractBytes(elf_path, entry_symbol, BYTES_TO_EXTRACT, symbol_bytes) != 0) {
        std::cerr << "Error: Failed to extract bytes from symbol '" << entry_symbol << "'" << std::endl;
        return -1;
    }
    
    if (symbol_bytes.empty()) {
        std::cerr << "Error: No bytes extracted from symbol '" << entry_symbol << "'" << std::endl;
        return -1;
    }
    
    // Search for the byte pattern in the flat binary
    std::vector<size_t> matches;
    auto it = flat_binary.begin();
    while ((it = std::search(it, flat_binary.end(), symbol_bytes.begin(), symbol_bytes.end())) != flat_binary.end()) {
        matches.push_back(std::distance(flat_binary.begin(), it));
        ++it; // Move past the current match to find others
    }
    
    // Check the results
    if (matches.empty()) {
        std::cerr << "Error: Symbol byte pattern not found in flat binary" << std::endl;
        return -1;
    }
    
    if (matches.size() > 1) {
        std::cerr << "Error: Symbol byte pattern found multiple times in flat binary at offsets: ";
        for (size_t i = 0; i < matches.size(); ++i) {
            std::cerr << "0x" << std::hex << matches[i] << std::dec;
            if (i < matches.size() - 1) std::cerr << ", ";
        }
        std::cerr << std::endl;
        return -1;
    }
    
    // Success - exactly one match found
    info.entry_binary_offset = matches[0];
    info.found_entry = true;
    info.entry_bytes = symbol_bytes; // Store the pattern for verification
    
    std::cout << "Entry symbol '" << entry_symbol << "' found at offset 0x" 
              << std::hex << info.entry_binary_offset << std::dec 
              << " in flat binary" << std::endl;
    
    return 0;
}

