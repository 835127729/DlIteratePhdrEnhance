//
// Created by chenkaiyi on 2024/5/12.
//

#include <string.h>
#include <sys/auxv.h>
#include "memory_elf.h"
#include "hook_linker.h"
#include "maps_visitor.h"
#include "include/dl_iterate_phdr_enhance.h"
#include "Log.h"
#include <inttypes.h>

#define TAG "dlfcn_bypass"

static int dl_iterate_phdr_by_maps_call(MemoryElf_t *memoryElf,
                                        dl_iterate_phdr_cb_t callback,
                                        void *data) {
    struct dl_phdr_info info;
    info.dlpi_name = memory_elf_get_file_path(memoryElf);
#ifdef DEBUG
    /**
     * 在Android Q（Android 10）之前，所有Android运行时组件（包括动态链接器）都位于/system/bin/linker64。
     * 然而，从Android Q开始，Android引入了APEX（Android Pony EXpress）系统，是一种新的系统组件更新机制，可以更灵活地更新核心系统组件，
     * 例如Android运行时。在这个新系统中，动态链接器位于 /apex/com.android.runtime/bin/linker64。
     * /system/bin/linker64仍然存在于Android Q及以后的版本中，但实际上它只是一个符号链接（symbolic link），
     * 指向实际的链接器/apex/com.android.runtime/bin/linker64。这样做的原因是为了确保对旧路径的引用仍然能够正确工作。
     * 因此，/apex/com.android.runtime/bin/linker64和/system/bin/linker64本质上都指向同一个动态链接器，只是路径和引用方式不同。
     */
    if (Env_getApiLevel() >= __ANDROID_API_Q__ &&
        strcmp(memory_elf_get_file_path(memoryElf),
               "/apex/com.android.runtime/bin/" LINKER_BASENAME) == 0) {
        info.dlpi_name = LINKER_PATH;
    }
#endif
    info.dlpi_phdr = memory_elf_get_phdr(memoryElf);
    info.dlpi_phnum = memory_elf_get_ehdr(memoryElf)->e_phnum;
    info.dlpi_addr = memory_elf_get_load_bias(memoryElf);
    return callback(&info, sizeof(info), data);
}

const char *string_util_get_extension(const char *filename) {
    const char *dot = strrchr(filename, '.'); // find the last dot
    if (!dot || dot == filename)
        return ""; // if there's no dot, or it's at the beginning, no extension
    return dot + 1; // pointer arithmetic: advances 1 past the dot
}

/**
 * 遍历maps查找内存elf，需要处理多种情况：
 * 例子1：
 * start_address = 727f0000, end_address = 727f3000, permission = r--p, offset = 0, major_dev = fe, minor_dev = 0, inode = 50d, path = /system/framework/arm64/boot-apache-xml.oat
 * start_address = 727f3000, end_address = 727f8000, permission = rw-p, offset = 0, major_dev = fe, minor_dev = 0, inode = 534, path = /system/framework/boot-apache-xml.vdex
 * start_address = 727f8000, end_address = 727f9000, permission = r--p, offset = 3000, major_dev = fe, minor_dev = 0, inode = 50d, path = /system/framework/arm64/boot-apache-xml.oat
 * start_address = 727f9000, end_address = 727fa000, permission = rw-p, offset = 4000, major_dev = fe, minor_dev = 0, inode = 50d, path = /system/framework/arm64/boot-apache-xml.oat
 *
 *
 */
int dl_iterate_phdr_by_maps(dl_iterate_phdr_cb_t callback, void *data) {
    LOGD(TAG, "dl_iterate_phdr_by_maps() start");
    MapVisitor_t *visitor = map_visitor_create(0);
    if (!map_visitor_valid(visitor)) {
        map_visitor_destroy(visitor);
        LOGE(TAG, "dl_iterate_phdr_by_maps() open maps visitor fail");
        return 0;
    }
    int result = 0;
    MemoryElf_t *preMemoryElf = NULL;
    while (map_visitor_has_next(visitor)) {
        MapItem mapItem = {0};
        if (map_visitor_next(visitor, &mapItem) != 0) {
            continue;
        }
        //检查读取权限
        if (mapItem.permission[0] != 'r' || mapItem.permission[3] != 'p') {
            continue;
        }
        //检查是否没有文件路径，是则可能是用于保护或隔离的预留空间
        if (mapItem.path[0] == '\0') {
            continue;
        }
        //跳过非文件路径的内存，但是[vdso]除外，因为原生dl_iterate_phdr也会返回这个区域
        if (mapItem.path[0] != '/' && strcmp(mapItem.path, VDSO_BASENAME) != 0) {
            continue;
        }
        //跳过一些已知的非法后缀，实际只有.so结尾才合法？
        const char *suffix = string_util_get_extension(mapItem.path);
        if ((strcmp(suffix, "jar") == 0) || (strcmp(suffix, "vdex") == 0)
            || (strcmp(suffix, "ttf")) == 0 || (strcmp(suffix, "ttc") == 0)) {
            continue;
        }

        //转换成内存elf
        MemoryElf_t *elf = memory_elf_create_by_handle((void *) (mapItem.start_address),
                                                       mapItem.path);

        /**
         * SO有可能是非linker加载到内存的，即通过mmap加载到内存的，这类内存需要跳过.
         * 如果当前内存是只读的，并且通过了elf文件头校验，那么它也可能是mmap加载的，
         * 需要等到找到可执行的mapItem，才能确认它是有linker加载的。
         * 因此先记录下来
         */
        if (memory_elf_is_valid(elf)) {
            if (preMemoryElf != NULL) {
                memory_elf_destroy(preMemoryElf);
                LOGD(TAG, "找到疑是mmap区域 %s", memory_elf_get_file_path(preMemoryElf));
            }
            preMemoryElf = elf;
            //对[vdso]进行特殊处理，只要遇到它，并检查通过，就需要回调
            if (strcmp(mapItem.path, VDSO_BASENAME) != 0) {
                continue;
            }
        }

        if (preMemoryElf == NULL) {
            memory_elf_destroy(elf);
            continue;
        }
        //检查前一个和当前的是否同名
        if (strcmp(memory_elf_get_file_path(preMemoryElf), mapItem.path) != 0) {
            memory_elf_destroy(elf);
            continue;
        }

        //回调前一个有效的
        result = dl_iterate_phdr_by_maps_call(preMemoryElf, callback, data);
        memory_elf_destroy(preMemoryElf);
        //释放引用
        preMemoryElf = NULL;
        if (result != 0) {
            //如果回调不是0，那么就结束dl_iterate_phdr遍历
            map_visitor_destroy(visitor);
            return result;
        }
    }
    if (preMemoryElf != NULL) {
        result = dl_iterate_phdr_by_maps_call(preMemoryElf, callback, data);
        memory_elf_destroy(preMemoryElf);
    }

    map_visitor_destroy(visitor);
    return result;
}

//记录linker的内存地址
static ElfW(Addr) linkerAddress = 0;

static int dl_iterate_phdr_linker_single(dl_iterate_phdr_cb_t callback, void *data) {
    if (getauxval == NULL) {
        return 0;
    }
    uintptr_t base = getauxval(AT_BASE);
    if (base == 0 || memcmp((void *) (base), ELFMAG, SELFMAG) != 0) {
        return 0;
    }
    //转换成内存elf
    MemoryElf_t *elf = memory_elf_create_by_handle((void *) (base), LINKER_PATH);
    if (memory_elf_is_valid(elf)) {
        struct dl_phdr_info info;
        info.dlpi_name = LINKER_PATH;
        info.dlpi_phdr = memory_elf_get_phdr(elf);
        info.dlpi_phnum = memory_elf_get_ehdr(elf)->e_phnum;
        info.dlpi_addr = memory_elf_get_load_bias(elf);
        //记录一下linker的地址
        linkerAddress = info.dlpi_addr;
        int result = callback(&info, sizeof(info), data);
        memory_elf_destroy(elf);
        return result;
    }
    memory_elf_destroy(elf);
    return 0;
}

static int
dl_iterate_phdr_by_linker_callback(struct dl_phdr_info *info, size_t size, void *dataArr) {
    uintptr_t *params = (uintptr_t *) (dataArr);
    dl_iterate_phdr_cb_t callback = (dl_iterate_phdr_cb_t) (params[0]);
    void *data = (void *) (params[1]);
    MapVisitor_t *mapVisitor = (MapVisitor_t *) (params[2]);

    /**
     * info->dlpi_addr为0，并且info->dlpi_name为NULL，则代表这个条目是程序的可执行文件本身，
     * 而非共享库。在某些实现中，可执行文件的 dlpi_name 被设置为一个空字符串或 NULL，而 dlpi_addr 通常是可执行文件映像的基地址，
     * 也可能是 0，具体是 0 还是其他值取决于该可执行文件在地址空间中的位置以及系统的内部实现
     */
    if (info->dlpi_addr == 0 || info->dlpi_name == NULL || info->dlpi_name[0] == '\0') {
        return callback(info, size, data);
    }

    /**
     * 由于当apiVersion < Android 8.1，原生dl_iterate_phdr不会遍历到linker/linker64.
     * 因此dl_iterate_phdr_linker_single()方法会遍历一次linker，这里是防止遍历两次linker
     */
    if (info->dlpi_addr == linkerAddress) {
        LOGV(TAG, "linkerAddress = %#" ADDRESS, linkerAddress);
        return 0;
    }

    /**
     * 这里参考https://github.com/hexhacking/xDL/的实现。
     * 不清楚什么情况下dl_iterate_phdr回调的info->dlpi_phdr、info->dlpi_phnum 会不合法。
     * 另外，直接将info->dlpi_addr强转成ElfW(Ehdr)也不恰当，这里是默认getMinAddress()会返回0。
     * 这里保留这段代码，但是不了解实现原理。
     */
    if (info->dlpi_phdr == NULL || info->dlpi_phnum == 0) {
        ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *) (info->dlpi_addr);
        info->dlpi_phdr = (ElfW(Phdr) *) (info->dlpi_addr + ehdr->e_phoff);
        info->dlpi_phnum = ehdr->e_phnum;
        LOGW(TAG, "dl_iterate_phdr_by_linker_callback 非法dl_phdr_info");
    }

    /**
     * 部分Android 4.x和Android 5.x设备的dl_iterate_phdr只能返回ELF的basename，而不是pathname
     */
    if (info->dlpi_name[0] != '/' && info->dlpi_name[0] != '[') {
        MemoryElf_t *elf = memory_elf_create_by_dl_info(info);
        uintptr_t base = (uintptr_t) (memory_elf_get_ehdr(elf));
        //重置文件流，避免打开多个maps文件导致fd溢出OOM
        map_visitor_reset(mapVisitor);
        while (map_visitor_has_next(mapVisitor)) {
            MapItem mapItem = {0};
            if (map_visitor_next(mapVisitor, &mapItem) != 0) {
                continue;
            }
            if (mapItem.start_address > base) {
                break;
            }
            if (mapItem.end_address <= base) {
                continue;
            }
            info->dlpi_name = mapItem.path;
            break;
        }
        memory_elf_destroy(elf);
    }

    return callback(info, size, data);
}

static int dl_iterate_phdr_by_system(dl_iterate_phdr_cb_t callback, void *data) {
    if (dl_iterate_phdr == NULL) {
        LOGW(TAG, "dl_iterate_phdr_by_system() dl_iterate_phdr is null");
        return 0;
    }

    /**
     * 当apiVersion < Android 8.1，原生dl_iterate_phdr不会遍历到linker/linker64.
     * AOSP从8.0开始已经包含了linker/linker64，但是大量的其他厂商的设备是从Android 8.1开始包含linker/linker64的.
     * 因此这里通过getauxval(AT_BASE)，主动找到linker/linker64
     */
    int result = dl_iterate_phdr_linker_single(callback, data);
    if (result != 0) {
        LOGE(TAG, "dl_iterate_phdr_by_system() result is %d", result);
        return result;
    }

    /**
     * 由于部分系统的原生dl_iterate_phdr存在缺陷，因此在callback给外部时，需要额外处理，
     * 因此这里使用dl_iterate_phdr_by_linker_callback包装一层
     */
    MapVisitor_t *mapVisitor = map_visitor_create(0);
    //这里把mapVisitor作为参数，避免打开多个maps文件导致fd溢出OOM
    if (!map_visitor_valid(mapVisitor)) {
        map_visitor_destroy(mapVisitor);
        LOGE(TAG, "dl_iterate_phdr_by_system() open maps visitor fail");
        return 0;
    }
    uintptr_t params[] = {(uintptr_t) callback, (uintptr_t) data, (uintptr_t) mapVisitor};
    const int apiLevel = android_get_device_api_level();
    /**
     * Android 5.0和5.1（API level 21和22），dl_iterate_phdr 的实现不持linker全局锁，
     * 需要自己找linker的符号（__dl__ZL10g_dl_mutex）自己加锁
     */
    if (__ANDROID_API_L__ == apiLevel || __ANDROID_API_L_MR1__ == apiLevel) {
        lockLinker();
    }
    result = dl_iterate_phdr(dl_iterate_phdr_by_linker_callback, params);
    if (__ANDROID_API_L__ == apiLevel || __ANDROID_API_L_MR1__ == apiLevel) {
        unLockLinker();
    }
    map_visitor_destroy(mapVisitor);
    return result;
}

/*
 * ====================================================================
 * API-LEVEL  ANDROID-VERSION  SOLUTION
 * ====================================================================
 * 16         4.1              /proc/self/maps
 * 17         4.2              /proc/self/maps
 * 18         4.3              /proc/self/maps
 * 19         4.4              /proc/self/maps
 * 20         4.4W             /proc/self/maps
 * --------------------------------------------------------------------
 * 21         5.0              dl_iterate_phdr() + __dl__ZL10g_dl_mutex
 * 22         5.1              dl_iterate_phdr() + __dl__ZL10g_dl_mutex
 * --------------------------------------------------------------------
 * >= 23      >= 6.0           dl_iterate_phdr()
 * ====================================================================
 */
int bypass_dl_iterate_phdr(dl_iterate_phdr_cb_t callback, void *data) {
    //如果当前编译的支持arm或x86，并且当前编译的API版本低于Android 5(L)，那么就需要支持maps查找的方式
    if (android_get_device_api_level() < __ANDROID_API_L__ || dl_iterate_phdr == NULL) {
        return dl_iterate_phdr_by_maps(callback, data);
    }
    //否则使用原生支持dl_iterate_phdr
    return dl_iterate_phdr_by_system(callback, data);
}