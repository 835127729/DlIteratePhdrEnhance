//
// Created by chenkaiyi on 2/5/25.
//

#pragma once

#include <sys/cdefs.h>
#include <link.h>
#include <elf.h>

__BEGIN_DECLS


typedef struct MemoryElf {
    const char *file_path;
    uintptr_t load_bias;
    const ElfW(Ehdr) *ehdr;
    const ElfW(Phdr) *phdr;
} MemoryElf;

MemoryElf *memory_elf_create(void *handle, const char *filePath);

void memory_elf_destroy(MemoryElf * memoryElf);

const ElfW(Ehdr) * get_ehdr_from_dl_phdr_info(struct dl_phdr_info* info);

__END_DECLS