#include "lle_manager.h"
#include "resource_map.h"
#include "error_codes.h"
#include <zephyr/llext/llext.h>
#include <zephyr/llext/buf_loader.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <elf.h>


#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lle_manager, LOG_LEVEL_DBG);

// Default configuration
#ifndef CONFIG_LLE_ENTRY_POINT_SYMBOL
#define CONFIG_LLE_ENTRY_POINT_SYMBOL "execute"
#endif

#ifndef CONFIG_LLE_NAME_PREFIX
#define CONFIG_LLE_NAME_PREFIX "kernel_"
#endif

int lle_manager_load_module(uintptr_t address, size_t size) {
    LOG_INF("Loading LLEXT module from address 0x%lx, size %zu", address, size);
    
    // Create buffer loader for the ELF module
    struct llext_buf_loader buf_loader = 
        LLEXT_BUF_LOADER((uint8_t*)address, size);
    struct llext_load_param ldr_param = LLEXT_LOAD_PARAM_DEFAULT;
    struct llext *ext;
    
    // Generate unique name based on address
    char ext_name[64];
    snprintf(ext_name, sizeof(ext_name), "kernel_0x%lx", address);
    
    // Load the LLEXT module
    int res = llext_load(&buf_loader.loader, ext_name, &ext, &ldr_param);
    if (res != 0) {
        LOG_ERR("Failed to load LLEXT module '%s', code %d", ext_name, res);
        return res;
    }
    
    // Update resource map with the loaded module
    resource_map_set_llext_handle(address, ext);
    resource_map_set_loaded_status(address, true);
    
    LOG_INF("Successfully loaded LLEXT module '%s'", ext_name);
    return 0;
}

void *lle_manager_get_entry_point(uintptr_t kernel_address) {
    LOG_DBG("Getting entry point for kernel at address 0x%lx", kernel_address);

    resource_entry_t* entry = resource_map_find_containing(kernel_address);
    if (!entry || !entry->is_lle_module) {
        LOG_ERR("No LLEXT module found containing address 0x%lx", kernel_address);
        return NULL;
    }

    struct llext *ext = resource_map_get_llext_handle(entry->address);
    if (!ext) {
        LOG_ERR("No LLEXT handle found for module at 0x%lx", entry->address);
        return NULL;
    }

    uintptr_t offset_from_base = kernel_address - entry->address;
    LOG_DBG("Module base: 0x%lx, kernel address: 0x%lx, offset: 0x%lx",
            entry->address, kernel_address, offset_from_base);

    const uint8_t *elf_base = (const uint8_t *)entry->address;
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_base;
    const Elf64_Shdr *shdr = (const Elf64_Shdr *)(elf_base + ehdr->e_shoff);
    const Elf64_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
    const char *shstrtab_data = (const char *)(elf_base + shstrtab->sh_offset);
    const Elf64_Sym *symtab = NULL;
    const char *strtab = NULL;
    size_t symtab_size = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *section_name = shstrtab_data + shdr[i].sh_name;
        if (shdr[i].sh_type == SHT_SYMTAB && strcmp(section_name, ".symtab") == 0) {
            symtab = (const Elf64_Sym *)(elf_base + shdr[i].sh_offset);
            symtab_size = shdr[i].sh_size / sizeof(Elf64_Sym);
        } else if (shdr[i].sh_type == SHT_STRTAB && strcmp(section_name, ".strtab") == 0) {
            strtab = (const char *)(elf_base + shdr[i].sh_offset);
        }
    }

    if (!symtab || !strtab) {
        LOG_ERR("Could not find symbol or string table in original ELF");
        return NULL;
    }

    uintptr_t min_vma = UINTPTR_MAX;
    for (size_t i = 0; i < symtab_size; i++) {
        if (ELF64_ST_TYPE(symtab[i].st_info) == STT_FUNC && symtab[i].st_value != 0) {
            if (symtab[i].st_value < min_vma) {
                min_vma = symtab[i].st_value;
            }
        }
    }

    if (min_vma == UINTPTR_MAX) {
        LOG_ERR("Could not find any function symbols in ELF");
        return NULL;
    }
    LOG_DBG("Calculated MinVMA: 0x%lx", min_vma);

    const char *target_symbol_name = NULL;
    for (size_t i = 0; i < symtab_size; i++) {
        if (ELF64_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
            uintptr_t symbol_offset = symtab[i].st_value - min_vma;
            if (symbol_offset == offset_from_base) {
                target_symbol_name = strtab + symtab[i].st_name;
                LOG_INF("Found matching symbol in original ELF: %s (st_value=0x%llx, offset=0x%lx)",
                        target_symbol_name, symtab[i].st_value, symbol_offset);
                break;
            }
        }
    }

    if (!target_symbol_name) {
        LOG_ERR("No function symbol found at offset 0x%lx in original ELF", offset_from_base);
        return NULL;
    }
  
    // find the relocated address of the kernel function itself.
    for (size_t i = 0; i < ext->sym_tab.sym_cnt; i++) {
        struct llext_symbol *sym = &ext->sym_tab.syms[i];
        if (sym->name && strcmp(sym->name, target_symbol_name) == 0) {
            LOG_INF("Found relocated kernel function: %s at %p", sym->name, sym->addr);
            // Return the address of the actual, relocated kernel function.
            return sym->addr; 
        }
    }

    LOG_ERR("Kernel symbol '%s' not found in LLEXT relocated symbols.", target_symbol_name);
    return NULL;
}

int lle_manager_init(void)
{
    LOG_INF("Initializing LLE manager");
    LOG_DBG("Using entry point symbol: '%s'", CONFIG_LLE_ENTRY_POINT_SYMBOL);
    LOG_DBG("Using name prefix: '%s'", CONFIG_LLE_NAME_PREFIX);
    // LLEXT subsystem should be initialized by Zephyr kernel
    LOG_DBG("LLE manager initialization complete");
    return 0;
}


bool lle_manager_unload_if_loaded(uintptr_t address)
{
    LOG_DBG("Checking if address 0x%lx has loaded LLE module", address);

    resource_entry_t* entry = resource_map_find(address);
    if (!entry || !entry->is_lle_module || !entry->is_loaded) {
        LOG_DBG("No loaded LLE module found at 0x%lx", address);
        return false;
    }

    struct llext *ext = resource_map_get_llext_handle(address);
    if (!ext) {
        LOG_WRN("LLE module at 0x%lx marked as loaded but no handle found", address);
        return false;
    }

    LOG_INF("Unloading LLE module at 0x%lx before buffer free", address);

    int res = llext_unload(&ext);
    if (res != 0) {
        LOG_ERR("Failed to unload LLE module at 0x%lx, return code %d", address, res);
        return false;
    }

    // Clear the handle and update status
    resource_map_set_llext_handle(address, NULL);
    resource_map_set_loaded_status(address, false);

    LOG_INF("Successfully unloaded LLE module at 0x%lx", address);
    return true;
}

void lle_manager_cleanup(void)
{
    LOG_INF("Cleaning up LLE manager");

    // TODO: Iterate through resource map and unload all LLEXT modules
    // This would require a resource_map_iterate() function or similar

    LOG_DBG("LLE manager cleanup complete");
}

