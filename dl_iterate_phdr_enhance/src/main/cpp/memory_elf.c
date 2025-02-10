//
// Created by chenkaiyi on 2/5/25.
//

#include <string.h>
#include <stdbool.h>
#include <sys/user.h>
#include <stdlib.h>
#include "memory_elf.h"
#include "Log.h"

// Returns the address of the page containing address 'x'.
static inline uintptr_t page_start(uintptr_t x) {
    return x & PAGE_MASK;
}

static ElfW(Addr) phdr_table_get_load_size(const ElfW(Phdr) *phdr, ElfW(Half) e_phnum) {
    ElfW(Addr) min_addr = UINTPTR_MAX;
    bool found_pt_load = false;
    for (int i = 0; i < e_phnum; i++) {
        const ElfW(Phdr)* phdr_i = &phdr[i];
        if(phdr_i->p_type != PT_LOAD){
            continue;
        }
        found_pt_load = true;
        if (phdr_i->p_vaddr < min_addr) {
            min_addr = phdr_i->p_vaddr;
        }
    }
    if(!found_pt_load){
        return 0;
    }
    min_addr = page_start(min_addr);
    return min_addr;
}

MemoryElf *memory_elf_create(void *handle, const char *filePath){
    if (filePath == NULL) {
        return NULL;
    }
    //检查ELF魔数
    if (memcmp(handle, ELFMAG, SELFMAG) != 0) {
        LOGE("MemoryElf", "MemoryElf() check magic number fail, filePath = %s", filePath);
        return NULL;
    }
    MemoryElf *memoryElf = (MemoryElf *) calloc(1, sizeof(MemoryElf));
    if (memoryElf == NULL) {
        return NULL;
    }
    memoryElf->file_path = strdup(filePath);
    if (memoryElf->file_path == NULL) {
        memory_elf_destroy(memoryElf);
        return NULL;
    }
    //初始化elf文件头和段表头
    memoryElf->ehdr = (const ElfW(Ehdr) *) handle;
    memoryElf->phdr = (const ElfW(Phdr) *) ((uintptr_t) handle + memoryElf->ehdr->e_phoff);
    memoryElf->load_bias = (uintptr_t) memoryElf->ehdr - phdr_table_get_load_size(memoryElf->phdr, memoryElf->ehdr->e_phnum);
    return memoryElf;
}

void memory_elf_destroy(MemoryElf* memoryElf){
    if(memoryElf == NULL){
        return;
    }
    free((void*)memoryElf->file_path);
    free(memoryElf);
}

const ElfW(Ehdr) * get_ehdr_from_dl_phdr_info(struct dl_phdr_info* info){
    ElfW(Addr) min_addr = phdr_table_get_load_size(info->dlpi_phdr, info->dlpi_phnum);
    if(min_addr == UINTPTR_MAX){
        return NULL;
    }
    const ElfW(Ehdr) * ehdr = (const ElfW(Ehdr) *) (min_addr +info->dlpi_addr);
    //检查魔数
    if (memcmp(ehdr, ELFMAG, SELFMAG) != 0) {
        return NULL;
    }
    return ehdr;
}