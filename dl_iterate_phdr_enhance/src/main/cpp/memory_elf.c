//
// Created by chenkaiyi on 2024/4/28.
//

#include <malloc.h>
#include <string.h>
#include "memory_elf.h"
#include "hash_util.h"
#include "include/dl_iterate_phdr_enhance.h"
#include "Log.h"
#include <sys/mman.h>

#define TAG "memory_elf"

typedef struct MemoryElf {
    char *filePath;
    uintptr_t load_bias;
    uint32_t flags;
    ElfW(Ehdr) *ehdr;
    const ElfW(Phdr) *phdr;
    ElfW(Dyn) *dynamic;
    ElfW(Sym) *dynsym;
    char *dynstr;
    SysvHash sysvHash;
    GnuHash gnuHash;
    RelPlt_t relplt;
    Rel_t relocation;
    RelAndroid_t androidRelocation;
    Needed needed;
} MemoryElf;

static inline bool is_symbol_global_and_defined(const ElfW(Sym) *s) {
    //必须是全局符号或者弱符号，并且是ELF自己定义的
    if (__predict_true(ELF_ST_BIND(s->st_info) == STB_GLOBAL ||
                       ELF_ST_BIND(s->st_info) == STB_WEAK)) {
        return s->st_shndx != SHN_UNDEF;
    } else if (__predict_false(ELF_ST_BIND(s->st_info) != STB_LOCAL)) {
//        DL_WARN("Warning: unexpected ST_BIND value: %d for \"%s\" in \"%s\" (ignoring)",
//                ELF_ST_BIND(s->st_info), si->get_string(s->st_name), si->get_realpath());
    }
    return false;
}

static ElfW(Addr) get_min_address(const ElfW(Phdr) *phdr, ElfW(Half) e_phnum) {
    ElfW(Addr) min_addr = UINTPTR_MAX;
    const ElfW(Phdr) *phdr_i = phdr;
    for (int i = 0; i < e_phnum; i++, phdr_i++) {
        if (phdr_i->p_type == PT_LOAD) {
            if (phdr_i->p_vaddr < min_addr) {
                min_addr = phdr_i->p_vaddr;
            }
        }
    }
    return min_addr;
}

MemoryElf_t *memory_elf_create_by_handle(void *handle, const char *filePath) {
    if (filePath == NULL) {
        return NULL;
    }
    //检查ELF魔数
    if (memcmp(handle, ELFMAG, SELFMAG) != 0) {
        LOGE(TAG, "MemoryElf() check magic number fail, filePath = %s", filePath);
        return NULL;
    }
    MemoryElf_t *memoryElf = (MemoryElf_t *) calloc(1, sizeof(MemoryElf_t));
    if (memoryElf == NULL) {
        return NULL;
    }
    memoryElf->filePath = strdup(filePath);
    if (memoryElf->filePath == NULL) {
        free(memoryElf);
        return NULL;
    }
    //初始化elf文件头和段表头
    memoryElf->load_bias = -1;
    memoryElf->ehdr = (ElfW(Ehdr) *) handle;
    memoryElf->phdr = (ElfW(Phdr) *) ((uintptr_t) handle + memoryElf->ehdr->e_phoff);
    return memoryElf;
}

MemoryElf_t *memory_elf_create_by_dl_info(struct dl_phdr_info *info) {
    //dlpi_addr为0,dlpi_name为NULL，说明是可执行文件本身，不是动态库
    if (info->dlpi_addr == 0 || info->dlpi_name == NULL) {
        return NULL;
    }
    MemoryElf_t *memoryElf = (MemoryElf_t *) calloc(1, sizeof(MemoryElf_t));
    if (memoryElf == NULL) {
        return NULL;
    }
    memoryElf->phdr = info->dlpi_phdr;
    memoryElf->load_bias = info->dlpi_addr;
    memoryElf->ehdr = (ElfW(Ehdr) *) (get_min_address(memoryElf->phdr, info->dlpi_phnum) +
                                      memoryElf->load_bias);
    //检查ELF魔数
    if (memcmp(memoryElf->ehdr, ELFMAG, SELFMAG) != 0) {
        LOGE(TAG, "MemoryElf() check magic number fail, filePath = %s", memoryElf->filePath);
        free(memoryElf);
        return NULL;
    }
    memoryElf->filePath = strdup(info->dlpi_name);
    if (memoryElf->filePath == NULL) {
        free(memoryElf);
        return NULL;
    }
    return memoryElf;
}

void memory_elf_destroy(MemoryElf_t *memoryElf) {
    if (memoryElf == NULL) {
        return;
    }
    free(memoryElf->filePath);
    free(memoryElf);
}

inline bool memory_elf_is_valid(MemoryElf_t *memoryElf) {
    return memoryElf != NULL && memoryElf->filePath != NULL;
}

uintptr_t memory_elf_get_load_bias(MemoryElf_t *memoryElf) {
    if (memoryElf->load_bias == -1) {
        //找到起始地址最小的段，计算内存偏移
        ElfW(Addr) minAddress = get_min_address(memoryElf->phdr, memoryElf->ehdr->e_phnum);
        if (minAddress == UINTPTR_MAX) {
            LOGE(TAG, "memory_elf_get_load_bias() can't find minAddress");
            return 0;
        }
        //计算真实虚拟内存地址和ELF目标虚拟地址的偏移
        memoryElf->load_bias = (uintptr_t) memoryElf->ehdr - minAddress;
    }
    return memoryElf->load_bias;
}

inline const char *memory_elf_get_file_path(MemoryElf_t *memoryElf) {
    return memoryElf->filePath;
}

inline const ElfW(Ehdr) *memory_elf_get_ehdr(MemoryElf_t *memoryElf) {
    return memoryElf->ehdr;
}

inline const ElfW(Phdr) *memory_elf_get_phdr(MemoryElf_t *memoryElf) {
    return memoryElf->phdr;
}

static ElfW(Addr) memory_elf_look_up_program_by_type(MemoryElf_t *memoryElf, ElfW(Word) p_type) {
    for (int i = 0; i < memoryElf->ehdr->e_phnum; i++) {
        const ElfW(Phdr) *phdr_i = memoryElf->phdr + i;
        if (phdr_i->p_type == p_type) {
            return memory_elf_get_load_bias(memoryElf) + phdr_i->p_vaddr;
        }
    }
    return UINTPTR_MAX;
}

static bool memory_elf_init_dynamic_program(MemoryElf_t *memoryElf) {
    if (memoryElf->dynamic != NULL) {
        return true;
    }
    //找到.dynamic
    ElfW(Addr) dynamicAddress = memory_elf_look_up_program_by_type(memoryElf, PT_DYNAMIC);
    if (dynamicAddress == UINTPTR_MAX) {
        return false;
    }
    memoryElf->dynamic = (ElfW(Dyn) *) dynamicAddress;
    return true;
}

static bool memory_elf_init_dynamic_symbol_table(MemoryElf_t *memoryElf) {
    if (!memory_elf_init_dynamic_program(memoryElf)) {
        return false;
    }
    if (memoryElf->dynsym != NULL) {
        return true;
    }
    //读取.dynamic
    for (ElfW(Dyn) *entry = memoryElf->dynamic; entry->d_tag != DT_NULL; entry++) {
        switch (entry->d_tag) {
            case DT_NEEDED:
                if (memoryElf->needed.cnt == memoryElf->needed.capacity) {
                    if (memoryElf->needed.so_name == NULL) {
                        memoryElf->needed.capacity = 2;
                    }
                    ElfW(Word) *new_array = realloc(memoryElf->needed.so_name,
                                                    (memoryElf->needed.capacity << 1) *
                                                    sizeof(ElfW(Word)));
                    if (new_array == NULL) {
                        LOGE(TAG, "memory_elf_init_dynamic_symbol_table() realloc fail");
                        break;
                    }
                    memoryElf->needed.so_name = new_array;
                    memoryElf->needed.capacity <<= 1;
                }
                memoryElf->needed.so_name[memoryElf->needed.cnt++] = entry->d_un.d_val;
                break;
            case DT_SYMTAB:
                memoryElf->dynsym = (ElfW(Sym) *) (memory_elf_get_load_bias(memoryElf) +
                                                   entry->d_un.d_ptr);
                break;
            case DT_STRTAB:
                memoryElf->dynstr = (char *) (memory_elf_get_load_bias(memoryElf) +
                                              entry->d_un.d_ptr);
                break;
            case DT_HASH:
                memoryElf->sysvHash.nbucket = ((uint32_t *) (memory_elf_get_load_bias(memoryElf) +
                                                             entry->d_un.d_ptr))[0];
                memoryElf->sysvHash.nchain = ((uint32_t *) (memory_elf_get_load_bias(memoryElf) +
                                                            entry->d_un.d_ptr))[1];
                memoryElf->sysvHash.bucket = (uint32_t *) (memory_elf_get_load_bias(memoryElf) +
                                                           entry->d_un.d_ptr + 8);
                memoryElf->sysvHash.chain = (uint32_t *) (memory_elf_get_load_bias(memoryElf) +
                                                          entry->d_un.d_ptr + 8 +
                                                          memoryElf->sysvHash.nbucket * 4);
                break;
            case DT_GNU_HASH:
                memoryElf->gnuHash.gnu_nbucket = ((uint32_t *) (
                        memory_elf_get_load_bias(memoryElf) + entry->d_un.d_ptr))[0];
                memoryElf->gnuHash.gnu_symndx = ((uint32_t *) (memory_elf_get_load_bias(memoryElf) +
                                                               entry->d_un.d_ptr))[1];
                memoryElf->gnuHash.gnu_maskwords = ((uint32_t *) (
                        memory_elf_get_load_bias(memoryElf) + entry->d_un.d_ptr))[2];
                memoryElf->gnuHash.gnu_shift2 = ((uint32_t *) (memory_elf_get_load_bias(memoryElf) +
                                                               entry->d_un.d_ptr))[3];
                memoryElf->gnuHash.gnu_bloom_filter = (ElfW(Addr) *) (
                        memory_elf_get_load_bias(memoryElf) + entry->d_un.d_ptr + 16);
                memoryElf->gnuHash.gnu_bucket = (uint32_t *) (memoryElf->gnuHash.gnu_bloom_filter +
                                                              memoryElf->gnuHash.gnu_maskwords);
                memoryElf->gnuHash.gnu_chain =
                        memoryElf->gnuHash.gnu_bucket + memoryElf->gnuHash.gnu_nbucket -
                        ((uint32_t *) (memory_elf_get_load_bias(memoryElf) + entry->d_un.d_ptr))[1];
                memoryElf->gnuHash.gnu_maskwords--;
                memoryElf->flags |= FLAG_GNU_HASH;
                break;
        }
    }
    return memoryElf->dynsym != NULL;
}

ElfW(Sym) *
memory_elf_look_up_symbol(MemoryElf_t *memoryElf, const char *symbolName, uint32_t *symbolIndex) {
    if (!memory_elf_init_dynamic_symbol_table(memoryElf)) {
        return NULL;
    }

    if ((memoryElf->flags & FLAG_GNU_HASH) == FLAG_GNU_HASH) {
        const uint32_t hash = gnu_hash(symbolName);
        const uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
        const uint32_t word_num = (hash / kBloomMaskBits) & memoryElf->gnuHash.gnu_maskwords;
        const ElfW(Addr) bloom_word = memoryElf->gnuHash.gnu_bloom_filter[word_num];
        const uint32_t h1 = hash % kBloomMaskBits;
        const uint32_t h2 = (hash >> memoryElf->gnuHash.gnu_shift2) % kBloomMaskBits;

        //查找非undef符号
        if ((1 & (bloom_word >> h1) & (bloom_word >> h2)) != 0) {
            uint32_t n = memoryElf->gnuHash.gnu_bucket[hash % memoryElf->gnuHash.gnu_nbucket];
            if (n != 0) {
                do {
                    //9、根据下标找到符号
                    ElfW(Sym) *s = memoryElf->dynsym + n;
                    //10、匹配符号hash是否相同，最低位是标记是否结束的，因此比较hash时需要忽略
                    if (((memoryElf->gnuHash.gnu_chain[n] ^ hash) >> 1) == 0 &&
                        //11、匹配符号是否相同，是则找到了
                        strcmp(memoryElf->dynstr + s->st_name, symbolName) == 0 &&
                        is_symbol_global_and_defined(s)) {
                        ElfW(Sym) *sym = memoryElf->dynsym + n;
                        if (symbolIndex != NULL) {
                            *symbolIndex = n;
                        }
                        return sym;
                    }
                    //12、否则查找下一个节点
                } while ((memoryElf->gnuHash.gnu_chain[n++] & 1) == 0);
            }
        }

        //undef的符号，遍历查找
        for (size_t i = 0; i < memoryElf->gnuHash.gnu_symndx; i++) {
            ElfW(Sym) *sym = memoryElf->dynsym + i;
            if (0 == strcmp(memoryElf->dynstr + sym->st_name, symbolName)) {
                if (symbolIndex != NULL) {
                    *symbolIndex = i;
                }
                return sym;
            }
        }
    } else {
        const uint32_t hash = elf_hash(symbolName);
        for (uint32_t n = memoryElf->sysvHash.bucket[hash % memoryElf->sysvHash.nbucket];
             n != 0; n = memoryElf->sysvHash.chain[n]) {
            ElfW(Sym) *sym = memoryElf->dynsym + n;
            if (strcmp(memoryElf->dynstr + sym->st_name, symbolName) == 0 &&
                is_symbol_global_and_defined(sym)) {
                if (symbolIndex != NULL) {
                    *symbolIndex = n;
                }
                return sym;
            }
        }
    }

    return NULL;
}