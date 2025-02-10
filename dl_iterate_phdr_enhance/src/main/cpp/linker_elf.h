//
// Created by chenkaiyi on 2024/4/28.
//
#pragma once

#include <link.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include "memory_elf.h"

__BEGIN_DECLS

typedef struct LinkerElf {
    MemoryElf* memoryElf;
    const ElfW(Shdr) *shdr;
    const ElfW(Sym) *symtab;
    size_t symCnt;
    const char *strtab;
    const char *shstrtab;
} LinkerElf;

LinkerElf *linker_elf_create(void *handle, const char *filePath);

void linker_elf_destroy(LinkerElf *linkerElf);

const ElfW(Sym) *linker_elf_look_up_symbol(LinkerElf *linkerElf, const char *symbolName);

__END_DECLS