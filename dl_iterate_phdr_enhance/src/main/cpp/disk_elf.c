////
//// Created by chenkaiyi on 2024/5/13.
////
//
//#include <malloc.h>
//#include <unistd.h>
//#include <elf.h>
//#include <string.h>
//#include <sys/mman.h>
//#include <fcntl.h>
//#include "disk_elf.h"
//#include "null_assert.h"
//#include "lzma.h"
//#include <pthread.h>
//#include <stdbool.h>
//#include <third_part/bsd/include/tree.h>
//
//typedef struct DiskElf {
//    char *filePath;
//    size_t size;
//    void *handle;
//    ElfW(Ehdr) *_ehdr;
//    ElfW(Shdr) *_shdr;
//    //.symtab
//    ElfW(Sym) *_symtab;
//    size_t _symtab_size;
//    size_t _symtab_cnt;
//    //.strtab
//    char *_strtab;
//    size_t _strtab_size;
//    //.dynsym
//    ElfW(Sym) *_dynsym;
//    size_t _dynsym_cnt;
//    //.dynstr
//    char *_dynstr;
//    //.shstrtab
//    char *_shstrtab;
//    //标记_strtab、_symtab是否来自.gnu_debugdata
//    bool from_gnu_debugdata;
//} DiskElf;
//
//typedef struct DiskElfEntry {
//    int fd;
//    void *handle;
//    size_t size;
//    size_t cnt;
//    const char *filePath;
//    RB_ENTRY(DiskElfEntry) entry;
//} DiskElfEntry_t;
//
//typedef RB_HEAD(diskElfTree, DiskElfEntry) diskElfTree_t;
//
//static int disk_elf_cmp(DiskElfEntry_t *e1, DiskElfEntry_t *e2) {
//    return strcmp(e1->filePath, e2->filePath);
//}
//
//RB_GENERATE_STATIC(diskElfTree, DiskElfEntry, entry, disk_elf_cmp)
//
//static pthread_once_t once = PTHREAD_ONCE_INIT;
//static diskElfTree_t diskElfCache;
//static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
//
//static void init_disk_elf_cache() {
//    RB_INIT(&diskElfCache);
//}
//
//static DiskElfEntry_t *
//disk_elf_entry_create(int fd, void *handle, size_t size, const char *filePath) {
//    DiskElfEntry_t *entry = (DiskElfEntry_t *) calloc(1, sizeof(DiskElfEntry_t));
//    NULL_RETURN_NULL(entry)
//    entry->fd = fd;
//    entry->handle = handle;
//    entry->size = size;
//    entry->cnt = 1;
//    entry->filePath = strdup(filePath);
//    return entry;
//}
//
//static int disk_elf_entry_destroy(DiskElfEntry_t *entry) {
//    if (entry == NULL) {
//        return -1;
//    }
//    free((void *) entry->filePath);
//    free(entry);
//    return 0;
//}
//
//static DiskElfEntry_t *find_disk_elf_from_cache(const char *filePath) {
//    pthread_once(&once, init_disk_elf_cache);
//    pthread_mutex_lock(&cache_mutex);
//    DiskElfEntry_t diskElfKey = {
//            .filePath = filePath,
//    };
//    DiskElfEntry_t *entry = RB_FIND(diskElfTree, &diskElfCache, &diskElfKey);
//    if (entry != NULL) {
//        entry->cnt++;
//        pthread_mutex_unlock(&cache_mutex);
//        return entry;
//    }
//
//    int fd = open(filePath, O_RDONLY);
//    if (fd < 0) {
//        pthread_mutex_unlock(&cache_mutex);
//        return NULL;
//    }
//    off_t size = lseek(fd, 0, SEEK_END);
//    if (size <= 0) {
//        pthread_mutex_unlock(&cache_mutex);
//        return NULL;
//    }
//    void *handle = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
//    if (handle == MAP_FAILED) {
//        pthread_mutex_unlock(&cache_mutex);
//        return NULL;
//    }
//    //检查elf魔数
//    if (memcmp(handle, ELFMAG, SELFMAG) != 0) {
//        pthread_mutex_unlock(&cache_mutex);
//        return NULL;
//    }
//    entry = disk_elf_entry_create(fd, handle, size, filePath);
//    RB_INSERT(diskElfTree, &diskElfCache, entry);
//
//    pthread_mutex_unlock(&cache_mutex);
//    return entry;
//}
//
//static int remove_disk_elf_from_cache(const char *filePath) {
//    pthread_once(&once, init_disk_elf_cache);
//    pthread_mutex_lock(&cache_mutex);
//    DiskElfEntry_t diskElfKey = {
//            .filePath = filePath,
//    };
//    DiskElfEntry_t *entry = RB_FIND(diskElfTree, &diskElfCache, &diskElfKey);
//    if (entry == NULL) {
//        pthread_mutex_unlock(&cache_mutex);
//        return -1;
//    }
//    entry->cnt--;
//    if (entry->cnt == 0) {
//        RB_REMOVE(diskElfTree, &diskElfCache, entry);
//        munmap(entry->handle, entry->size);
//        close(entry->fd);
//        disk_elf_entry_destroy(entry);
//    }
//    pthread_mutex_unlock(&cache_mutex);
//    return 0;
//}
//
//DiskElf_t *disk_elf_create_by_file_path(const char *filePath) {
//    DiskElf_t *diskElf = (DiskElf_t *) calloc(1, sizeof(DiskElf_t));
//    if (diskElf == NULL) {
//        return NULL;
//    }
//    DiskElfEntry_t *entry = find_disk_elf_from_cache(filePath);
//    if (entry == NULL) {
//        free(diskElf);
//        return NULL;
//    }
//    diskElf->handle = entry->handle;
//    diskElf->filePath = entry->filePath;
//    diskElf->size = entry->size;
//    //读取ehdr
//    diskElf->_ehdr = (ElfW(Ehdr) *) diskElf->handle;
//    //读取shdr
//    diskElf->_shdr = (ElfW(Shdr) *) ((uintptr_t) diskElf->handle + diskElf->_ehdr->e_shoff);
//    return diskElf;
//}
//
//int disk_elf_destroy(DiskElf_t *diskElf) {
//    if (remove_disk_elf_from_cache(diskElf->filePath) != 0) {
//        return -1;
//    }
//    free(diskElf);
//    if (diskElf->from_gnu_debugdata) {
//        free(diskElf->_strtab);
//        free(diskElf->_symtab);
//    }
//    return 0;
//}
//
//bool disk_elf_is_valid(DiskElf_t *diskElf) {
//    return diskElf->_ehdr != NULL;
//}
//
//
//static bool disk_elf_init_section_string_table(DiskElf_t *diskElf) {
//    if (diskElf == NULL) {
//        return false;
//    }
//    if (diskElf->_shstrtab != NULL) {
//        return true;
//    }
//    ElfW(Shdr) *strShdr = diskElf->_shdr;
//    ElfW(Shdr) *shstrShdr = strShdr + diskElf->_ehdr->e_shstrndx;
//    diskElf->_shstrtab = ((char *) diskElf->handle) + shstrShdr->sh_offset;
//    return true;
//}
//
//static const char *disk_elf_get_section_name(DiskElf_t *diskElf, ElfW(Word) offset) {
//    if (!disk_elf_init_section_string_table(diskElf)) {
//        return NULL;
//    }
//    return diskElf->_shstrtab + offset;
//}
//
//static ElfW(Shdr) *disk_elf_look_up_section_by_name(DiskElf_t *diskElf, const char *name) {
//    for (size_t i = 0; i < diskElf->_ehdr->e_shnum; i++) {
//        ElfW(Shdr) *shdr = diskElf->_shdr + i;
//        if (strcmp(disk_elf_get_section_name(diskElf, shdr->sh_name), name) == 0) {
//            return shdr;
//        }
//    }
//    return NULL;
//}
//
//static ElfW(Shdr) *disk_elf_look_up_section_by_type(DiskElf_t *diskElf, ElfW(Word) type) {
//    for (size_t i = 0; i < diskElf->_ehdr->e_shnum; i++) {
//        ElfW(Shdr) *shdr = diskElf->_shdr + i;
//        if (shdr->sh_type == type) {
//            return shdr;
//        }
//    }
//    return NULL;
//}
//
////符号表中记录了对应的字符串表的序号，可以避免遍历查找
//static ElfW(Shdr) *
//disk_elf_look_up_string_table_for_symbol_table(DiskElf_t *diskElf, ElfW(Shdr) *symbol_shdr) {
//    if (symbol_shdr == NULL) {
//        return NULL;
//    }
//    if (symbol_shdr->sh_type != SHT_SYMTAB && symbol_shdr->sh_type != SHT_DYNSYM) {
//        return NULL;
//    }
//    //获取对应的字符串表
//    if (symbol_shdr->sh_link >= diskElf->_ehdr->e_shnum) {
//        //没有对应的字符串表，非法
//        return NULL;
//    }
//    ElfW(Shdr) *string_shdr = diskElf->_shdr + symbol_shdr->sh_link;
//    if (string_shdr->sh_type != SHT_STRTAB) {
//        return NULL;
//    }
//    return string_shdr;
//}
//
//static bool disk_elf_init_dynamic_symbol_table(DiskElf_t *diskElf) {
//    if (diskElf == NULL) {
//        return false;
//    }
//    if (diskElf->_dynsym != NULL && diskElf->_dynstr != NULL) {
//        return true;
//    }
//
//    ElfW(Shdr) *symbol_shdr = disk_elf_look_up_section_by_type(diskElf, SHT_DYNSYM);
//    if (symbol_shdr == NULL) {
//        return false;
//    }
//    ElfW(Shdr) *string_shdr = disk_elf_look_up_string_table_for_symbol_table(diskElf, symbol_shdr);
//    if (string_shdr == NULL) {
//        return false;
//    }
//    diskElf->_dynsym = (ElfW(Sym) *) ((uintptr_t) diskElf->handle + symbol_shdr->sh_offset);
//    diskElf->_dynstr = (char *) diskElf->handle + string_shdr->sh_offset;
//    diskElf->_dynsym_cnt =
//            symbol_shdr->sh_entsize <= 0 ? 0 : symbol_shdr->sh_size / symbol_shdr->sh_entsize;
//    return true;
//}
//
//static bool disk_elf_init_symbol_table(DiskElf_t *diskElf) {
//    if (diskElf == NULL) {
//        return false;
//    }
//    if (diskElf->_symtab != NULL && diskElf->_strtab != NULL) {
//        return true;
//    }
//    ElfW(Shdr) *symbol_shdr = disk_elf_look_up_section_by_type(diskElf, SHT_SYMTAB);
//    if (symbol_shdr == NULL) {
//        return false;
//    }
//    ElfW(Shdr) *string_shdr = disk_elf_look_up_string_table_for_symbol_table(diskElf, symbol_shdr);
//    if (string_shdr == NULL) {
//        return false;
//    }
//    diskElf->_symtab = (ElfW(Sym) *) ((uintptr_t) diskElf->handle + symbol_shdr->sh_offset);
//    diskElf->_symtab_size = symbol_shdr->sh_size;
//    diskElf->_symtab_cnt =
//            symbol_shdr->sh_entsize <= 0 ? 0 : symbol_shdr->sh_size / symbol_shdr->sh_entsize;
//    diskElf->_strtab = (char *) diskElf->handle + string_shdr->sh_offset;
//    diskElf->_strtab_size = string_shdr->sh_size;
//    return true;
//}
//
//static bool disk_elf_init_string_table(DiskElf_t *diskElf) {
//    if (diskElf == NULL) {
//        return false;
//    }
//    if (diskElf->_strtab != NULL) {
//        return true;
//    }
//    ElfW(Shdr) *string_shdr = disk_elf_look_up_section_by_name(diskElf, ".strtab");
//    if (string_shdr == NULL || string_shdr->sh_type != SHT_STRTAB) {
//        return false;
//    }
//    diskElf->_strtab = ((char *) diskElf->handle) + string_shdr->sh_offset;
//    diskElf->_strtab_size = string_shdr->sh_size;
//    return true;
//}
//
//static DiskElf_t *disk_elf_create_from_gnu_debug_data(void *handle, size_t size) {
//    DiskElf_t *diskElf = (DiskElf_t *) calloc(1, sizeof(DiskElf_t));
//    if (diskElf == NULL) {
//        return NULL;
//    }
//    diskElf->handle = handle;
//    diskElf->filePath = NULL;
//    diskElf->size = size;
//    //读取ehdr
//    diskElf->_ehdr = (ElfW(Ehdr) *) diskElf->handle;
//    //读取shdr
//    diskElf->_shdr = (ElfW(Shdr) *) ((uintptr_t) diskElf->handle + diskElf->_ehdr->e_shoff);
//    return diskElf;
//}
//
///**
// * SO可能没有.symtab和.strtab，只有.dynsym和.dynstr。
// * 在这种情况下，可以尝试从.gnu_debugdata节中读取。
// * .gnu_debugdata节不会被加载到内存中，因此只能读取ELF文件来加载。
// * .gnu_debugdata节的内容通过lzma解压缩后，内容也为ELF文件格式，因此可以按照ELF文件格式加载。
// *  解压后的内容中，通常有.symtab、.strtab、.shstrtab、.text节。
// *  其中的.symtab、.strtab，实际是当前ELF文件的节。从中.symtab读取的符号值，就是当前ELF文件的符号值。
// *  因此可以说，当前ELF文件的.symtab、.strtab节，是从.gnu_debugdata节读取出来的。
// */
//static bool disk_elf_init_gnu_debugdata(DiskElf_t *diskElf) {
//    //找到gnu_debugdata段
//    ElfW(Shdr) *gnu_debugdata_shdr = disk_elf_look_up_section_by_name(diskElf, ".gnu_debugdata");
//    if (gnu_debugdata_shdr == NULL) {
//        return false;
//    }
//
//    // get zipped .gnu_debugdata
//    void *debugdata_zip = (uint8_t *) (diskElf->handle + gnu_debugdata_shdr->sh_offset);
//
//    void *debugdata = NULL;
//    size_t debugdata_sz = 0;
//    //解压缩
//    if (elf_lzma_decompress(debugdata_zip, gnu_debugdata_shdr->sh_size, (uint8_t **) &debugdata,
//                            &debugdata_sz) != 0) {
//        return false;
//    }
//    //解压后内容格式仍未ELF
//    DiskElf_t *disk_elf_debug_data = disk_elf_create_from_gnu_debug_data(debugdata, debugdata_sz);
//    if (disk_elf_debug_data == NULL || !disk_elf_is_valid(disk_elf_debug_data)) {
//        return false;
//    }
//    //复制.symtab
//    if (disk_elf_init_symbol_table(disk_elf_debug_data)) {
//        ElfW(Sym) *symtab = malloc(disk_elf_debug_data->_symtab_size);
//        if (symtab != NULL &&
//            memcpy(symtab, disk_elf_debug_data->_symtab, disk_elf_debug_data->_symtab_size) !=
//            NULL) {
//            diskElf->_symtab = symtab;
//            diskElf->_symtab_size = disk_elf_debug_data->_symtab_size;
//            diskElf->_symtab_cnt = disk_elf_debug_data->_symtab_cnt;
//        }
//    }
//    //复制.strtab
//    if (disk_elf_init_string_table(disk_elf_debug_data)) {
//        char *strtab = malloc(disk_elf_debug_data->_strtab_size);
//        if (strtab != NULL &&
//            memcpy(strtab, disk_elf_debug_data->_strtab, disk_elf_debug_data->_strtab_size) !=
//            NULL) {
//            diskElf->_strtab = strtab;
//            diskElf->_strtab_size = disk_elf_debug_data->_strtab_size;
//        }
//    }
//
//    //标记.strtab、.symtab内存来源，用于后续释放
//    diskElf->from_gnu_debugdata = true;
//    //最后，释放debugdata内存，因为原始debugdata有多余.text节等内存占用
//    free(debugdata);
//    free(disk_elf_debug_data);
//    return true;
//}
//
//static ElfW(Sym) *
//disk_elf_look_up_in_symbol_table(ElfW(Sym) *symtab, size_t symbol_cnt, char *strtab,
//                                 const char *symbol_name,
//                                 uint32_t *symbol_index) {
//    for (size_t i = 0; i < symbol_cnt; i++) {
//        ElfW(Sym) *symbol = symtab + i;
//        char *name = strtab + symbol->st_name;
//        if (strcmp(name, symbol_name) == 0) {
//            if (symbol_index != NULL) {
//                *symbol_index = i;
//            }
//            return symbol;
//        }
//    }
//    return NULL;
//}
//
//ElfW(Sym) *
//disk_elf_look_up_dynamic_symbol(DiskElf_t *diskElf, const char *symbol_name,
//                                uint32_t *symbol_index) {
//    if (!disk_elf_init_dynamic_symbol_table(diskElf)) {
//        return NULL;
//    }
//    return disk_elf_look_up_in_symbol_table(diskElf->_dynsym, diskElf->_dynsym_cnt,
//                                            diskElf->_dynstr,
//                                            symbol_name, symbol_index);
//}
//
//ElfW(Sym) *
//disk_elf_look_up_symbol(DiskElf_t *diskElf, const char *symbolName, uint32_t *symbolIndex) {
//    //正常查找
//    if (disk_elf_init_symbol_table(diskElf) && disk_elf_init_string_table(diskElf)) {
//        ElfW(Sym) *symbol = disk_elf_look_up_in_symbol_table(diskElf->_symtab, diskElf->_symtab_cnt,
//                                                             diskElf->_strtab,
//                                                             symbolName, symbolIndex);
//        if (symbol != NULL) {
//            return symbol;
//        }
//    }
//
//    //从.gnu_debugdata查找
//    if (disk_elf_init_gnu_debugdata(diskElf)) {
//        ElfW(Sym) *symbol = disk_elf_look_up_in_symbol_table(diskElf->_symtab, diskElf->_symtab_cnt,
//                                                             diskElf->_strtab,
//                                                             symbolName, symbolIndex);
//        if (symbol != NULL) {
//            return symbol;
//        }
//    }
//    return NULL;
//}