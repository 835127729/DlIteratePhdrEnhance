//
// Created by chenkaiyi on 2024/4/28.
//

#include <malloc.h>
#include <string.h>
#include "linker_elf.h"
#include "include/dl_iterate_phdr_enhance.h"
#include "Log.h"
#include "hook_linker.h"
#include "memory_elf.h"
#include <sys/mman.h>

#define TAG "memory_elf"

#define SYMTAB_IS_EXPORT_SYM(shndx) \
  (SHN_UNDEF != (shndx) && !((shndx) >= SHN_LORESERVE && (shndx) <= SHN_HIRESERVE))

static const ElfW(Shdr)* findTargetSection(LinkerElf *linkerElf, ElfW(Word) type, const char* name) {
    for(size_t i = 0; i < linkerElf->memoryElf->ehdr->e_shnum; i++) {
        const ElfW(Shdr) *shdr = linkerElf->shdr + i;
        if (shdr->sh_type != type) {
            continue;
        }
        if(linkerElf->shstrtab == NULL || strcmp(linkerElf->shstrtab + shdr->sh_name, name) == 0){
            return shdr;
        }
    }
    return NULL;
}

static void* allocSection(const ElfW(Shdr)* shdr, FILE* file){
    if(shdr == NULL){
        return NULL;
    }
    if(fseeko(file, shdr->sh_offset, SEEK_SET) != 0){
        return NULL;
    }
    char* section = malloc(shdr->sh_size);
    if(section == NULL){
        return NULL;
    }
    if(fread(section, 1, shdr->sh_size, file) != shdr->sh_size){
        free(section);
        return NULL;
    }
    return section;
}

static void initStringTable(LinkerElf *linkerElf, FILE* file) {
    const ElfW(Shdr)* shdr = findTargetSection(linkerElf, SHT_STRTAB, ".strtab");
    linkerElf->strtab =  allocSection(shdr, file);
}

static void initSectionStringTable(LinkerElf *linkerElf, FILE* file) {
    if(linkerElf->memoryElf->ehdr->e_shstrndx == SHN_UNDEF){
        return;
    }
    const ElfW(Shdr) *shstrtab = linkerElf->shdr + linkerElf->memoryElf->ehdr->e_shstrndx;
    linkerElf->shstrtab = allocSection(shstrtab, file);
}

static void initSymbolTable(LinkerElf *linkerElf, FILE* file){
    const ElfW(Shdr)* shdr = findTargetSection(linkerElf, SHT_SYMTAB, ".symtab");
    void* symtab = allocSection(shdr, file);
    if(symtab == NULL){
        return;
    }
    linkerElf->symtab = symtab;
    linkerElf->symCnt = shdr->sh_entsize == 0 ? 0 : shdr->sh_size / shdr->sh_entsize;
}

static void initSectionTable(LinkerElf *linkerElf, FILE* file) {
    if(fseek(file, linkerElf->memoryElf->ehdr->e_shoff, SEEK_SET) != 0){
        return;
    }
    size_t size = linkerElf->memoryElf->ehdr->e_shentsize * linkerElf->memoryElf->ehdr->e_shnum;
    if(size == 0){
        return;
    }
    ElfW(Shdr)* shdr = malloc(size);
    if(shdr == NULL){
        return;
    }
    if(fread(shdr, 1, size, file) != size){
        free(shdr);
        return;
    }
    linkerElf->shdr = shdr;
}

LinkerElf *linker_elf_create(void *handle, const char *filePath) {
    if (filePath == NULL) {
        return NULL;
    }
    //检查ELF魔数
    if (memcmp(handle, ELFMAG, SELFMAG) != 0) {
        LOGE(TAG, "LinkerElf() check magic number fail, filePath = %s", filePath);
        return NULL;
    }
    LinkerElf *linkerElf = (LinkerElf *) calloc(1, sizeof(LinkerElf));
    if (linkerElf == NULL) {
        return NULL;
    }
    linkerElf->memoryElf = memory_elf_create(handle, filePath);
    if (linkerElf->memoryElf == NULL) {
        linker_elf_destroy(linkerElf);
        return NULL;
    }
    //将Linker读取到内存
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        linker_elf_destroy(linkerElf);
        return NULL;
    }
    //初始化section table
    initSectionTable(linkerElf, file);
    if(linkerElf->shdr == NULL){
        fclose(file);
        linker_elf_destroy(linkerElf);
        return NULL;
    }
    //Section名称字符串表
    initSectionStringTable(linkerElf, file);
    //字符串表
    initStringTable(linkerElf, file);
    if(linkerElf->strtab == NULL){
        fclose(file);
        linker_elf_destroy(linkerElf);
        return NULL;
    }
    //符号表
    initSymbolTable(linkerElf, file);
    if(linkerElf->symtab == NULL){
        fclose(file);
        linker_elf_destroy(linkerElf);
        return NULL;
    }
    fclose(file);
    return linkerElf;
}

void linker_elf_destroy(LinkerElf *linkerElf) {
    if (linkerElf == NULL) {
        return;
    }
    memory_elf_destroy(linkerElf->memoryElf);
    free((void*)linkerElf->strtab);
    free((void*)linkerElf->symtab);
    free((void*)linkerElf->shdr);
    free(linkerElf);
}

const ElfW(Sym) *linker_elf_look_up_symbol(LinkerElf *linkerElf, const char *symbolName){
    //遍历符号表
    for(size_t i = 0; i < linkerElf->symCnt; i++){
        const ElfW(Sym) * symbol = linkerElf->symtab + i;
        if(!SYMTAB_IS_EXPORT_SYM(symbol->st_shndx)){
            continue;
        }
        const char* name = linkerElf->strtab + symbol->st_name;
        if(strcmp(name, symbolName) == 0) {
            return symbol;
        }
    }
    return NULL;
}