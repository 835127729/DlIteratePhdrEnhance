//
// Created by chenkaiyi on 2024/4/28.
//
#pragma once

#include <link.h>
#include "hook_elf.h"
#include <inttypes.h>
#include <stdbool.h>

#define FLAG_RELA             0x00000008 // use .rela
#define FLAG_ANDROID_REL      0x00000010 // use .relandroid
#define FLAG_GNU_HASH         0x00000040 // uses gnu hash

#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * 根据源码：https://cs.android.com/android/platform/superproject/+/android-5.0.0_r1.0.1:bionic/linker/linker.h
 * Android在5.0以后，支持64位cpu，并且就对应64位CPU使用rela格式，因此只要是64位就使用rela
 */
#if defined(__LP64__)
typedef ElfW(Rela) Relc;
#else
typedef ElfW(Rel) Relc;
#endif


typedef struct {
    size_t nbucket;
    size_t nchain;
    uint32_t *bucket;
    uint32_t *chain;
} SysvHash;

typedef struct {
    size_t gnu_nbucket;
    size_t gnu_symndx;
    uint32_t *gnu_bucket;
    uint32_t *gnu_chain;
    uint32_t gnu_maskwords;
    uint32_t gnu_shift2;
    ElfW(Addr) *gnu_bloom_filter;
} GnuHash;

typedef struct {
    Relc *relplt;
    ElfW(Word) relplt_sz;
    ElfW(Word) relplt_ent;
} RelPlt_t;

typedef struct {
    Relc *reldyn;
    ElfW(Word) reldyn_sz;
    ElfW(Word) reldyn_ent;
} Rel_t;

typedef struct {
    ElfW(Addr) rel_android; //android compressed rel or rela
    ElfW(Word) rel_android_sz;
} RelAndroid_t;

typedef struct {
    size_t capacity;
    size_t cnt;
    ElfW(Word) *so_name;
} Needed;

typedef struct MemoryElf MemoryElf_t;

MemoryElf_t *memory_elf_create_by_handle(void *handle, const char *filePath);

MemoryElf_t *memory_elf_create_by_dl_info(struct dl_phdr_info *info);

MemoryElf_t *memory_elf_create_by_lib_name(const char *libName);

void memory_elf_destroy(MemoryElf_t *memoryElf);

bool memory_elf_is_valid(MemoryElf_t *memoryElf);

uintptr_t memory_elf_get_load_bias(MemoryElf_t *memoryElf);

const char *memory_elf_get_file_path(MemoryElf_t *memoryElf);

const ElfW(Ehdr) *memory_elf_get_ehdr(MemoryElf_t *memoryElf);

const ElfW(Phdr) *memory_elf_get_phdr(MemoryElf_t *memoryElf);

const char *memory_elf_get_dyn_string(MemoryElf_t *memoryElf, ElfW(Word) offset);

ElfW(Sym) *
memory_elf_look_up_symbol(MemoryElf_t *memoryElf, const char *symbolName, uint32_t *symbolIndex);

int
memory_elf_look_up_rel_plt(MemoryElf_t *memoryElf, bool (*filter)(Relc *relc, void *args),
                           void *args);

Instruction *
memory_elf_look_up_instruction(MemoryElf_t *memoryElf, const Instruction *instruction,
                               size_t length);

int memory_elf_get_protect_by_address(MemoryElf_t *memoryElf, uintptr_t address);

Needed *memory_elf_get_needed(MemoryElf_t *memoryElf);

__END_DECLS