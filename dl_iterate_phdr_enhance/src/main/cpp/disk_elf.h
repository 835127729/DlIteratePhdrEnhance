////
//// Created by chenkaiyi on 2024/5/13.
////
//#pragma once
//
//#include <stdio.h>
//#include <link.h>
//#include <stdbool.h>
//
//#include <sys/cdefs.h>
//
//__BEGIN_DECLS
//
//typedef struct DiskElf DiskElf_t;
//
//DiskElf_t *disk_elf_create_by_file_path(const char *filePath);
//
//int disk_elf_destroy(DiskElf_t *diskElf);
//
//bool disk_elf_is_valid(DiskElf_t *diskElf);
//
//ElfW(Sym) *
//disk_elf_look_up_dynamic_symbol(DiskElf_t *diskElf, const char *symbol_name,
//                                uint32_t *symbol_index);
//
//ElfW(Sym) *
//disk_elf_look_up_symbol(DiskElf_t *diskElf, const char *symbolName, uint32_t *symbolIndex);
//
//__END_DECLS