//
// Created by chenkaiyi on 2024/5/12.
//

#include <string.h>
#include "linker_elf.h"
#include "hook_linker.h"
#include "maps_visitor.h"
#include "include/dl_iterate_phdr_enhance.h"
#include "Log.h"
#include "link.h"
#include "memory_elf.h"
#include <inttypes.h>
#include <sys/auxv.h>
#define TAG "dl_iterate_phdr_enhance"

static int dl_iterate_phdr_callback_memory_elf(MemoryElf *memoryElf,
                                               dl_iterate_phdr_cb_t callback,
                                               void *data) {
    struct dl_phdr_info info = {0};
    info.dlpi_phdr = memoryElf->phdr;
    info.dlpi_phnum = memoryElf->ehdr->e_phnum;
    info.dlpi_addr = memoryElf->load_bias;
    info.dlpi_name = memoryElf->file_path;
    return callback(&info, sizeof(info), data);
}

/**
 * 遍历maps查找内存elf，需要处理多种情况：
 * 例子1：
 * start_address = 727f0000, end_address = 727f3000, permission = r--p, offset = 0, major_dev = fe, minor_dev = 0, inode = 50d, path = /system/framework/arm64/boot-apache-xml.oat
 * start_address = 727f3000, end_address = 727f8000, permission = rw-p, offset = 0, major_dev = fe, minor_dev = 0, inode = 534, path = /system/framework/boot-apache-xml.vdex
 * start_address = 727f8000, end_address = 727f9000, permission = r--p, offset = 3000, major_dev = fe, minor_dev = 0, inode = 50d, path = /system/framework/arm64/boot-apache-xml.oat
 * start_address = 727f9000, end_address = 727fa000, permission = rw-p, offset = 4000, major_dev = fe, minor_dev = 0, inode = 50d, path = /system/framework/arm64/boot-apache-xml.oat
 *
 * 例子2：
70000000-70a37000 rw-p 00000000 fe:00 686                                /system/framework/arm64/boot.art
70a37000-724fe000 r--p 00000000 fe:00 687                                /system/framework/arm64/boot.oat
724fe000-744d4000 r-xp 01ac7000 fe:00 687                                /system/framework/arm64/boot.oat
744d4000-744d5000 rw-p 03a9d000 fe:00 687                                /system/framework/arm64/boot.oat
...
7b509f4000-7b50a05000 r-xp 00000000 fe:00 270                            /system/bin/linker64
7b50a05000-7b50a06000 r--p 00000000 00:00 0                              [anon:linker_alloc]
7b50a06000-7b50a08000 rw-p 00000000 00:01 2231                           /dev/ashmem/dalvik-mark sweep sweep array free buffer (deleted)
7b50a08000-7b50a09000 r--p 00000000 fe:00 687                            /system/framework/arm64/boot.oat
7b50a09000-7b50a0b000 r--p 00000000 00:00 0                              [anon:linker_alloc]

 */
int dl_iterate_phdr_by_maps(dl_iterate_phdr_cb_t callback, void *data) {
    LOGD(TAG, "dl_iterate_phdr_by_maps() start");
    MapsVisitor_t *visitor = maps_visitor_create(0);
    if (!maps_visitor_valid(visitor)) {
        maps_visitor_destroy(visitor);
        LOGE(TAG, "dl_iterate_phdr_by_maps() open maps visitor fail");
        return 0;
    }
    int result = 0;
    MapItem map_item_1 = {0};
    MapItem map_item_2 = {0};
    MapItem *current_map_item = &map_item_1;
    bool try_next_line = false;

    lockLinker();
    while (maps_visitor_has_next(visitor)) {
        if (maps_visitor_next(visitor, current_map_item) == NULL) {
            continue;
        }

        if(current_map_item->permission[2] == '-' && 0 == current_map_item->offset){
            /**
             * 权限为r--p，并且偏移量为0，说明可能是ELF文件的开头，也可能是其他类型文件，
             * 需要找到一个权限为可执行的段才能进一步确认。
             */
            current_map_item = current_map_item == &map_item_1 ? &map_item_2 : &map_item_1;
            try_next_line = true;
            continue;
        }else if(current_map_item->permission[2] == 'x'){
            uintptr_t offset = current_map_item->offset;
            uintptr_t start_address = current_map_item->start_address;
            //r-xp
            if(try_next_line && current_map_item->offset != 0){
                MapItem *pre_map_item = current_map_item == &map_item_1 ? &map_item_2 : &map_item_1;
                //检查前一行和当前的是否同名
                if(strcmp(pre_map_item->path, current_map_item->path) != 0){
                    goto clean;
                }
                offset = 0;
                start_address = pre_map_item->start_address;
            }
            //不是文件起始位置，检查是否没有文件路径，
            if(offset != 0 || current_map_item->path[0] == '\0'){
                goto clean;
            }

            MemoryElf *elf = memory_elf_create((void *) (start_address),current_map_item->path);
            if (elf == NULL) {
                memory_elf_destroy(elf);
                goto clean;
            }
            result = dl_iterate_phdr_callback_memory_elf(elf, callback, data);
            memory_elf_destroy(elf);
            if(result != 0){
                break;
            }
        }
        clean:
            try_next_line = false;
    }
    maps_visitor_destroy(visitor);
    return result;
}

static int dl_iterate_phdr_linker_single(dl_iterate_phdr_cb_t callback, void *data) {
    uintptr_t base = linkerBase();
    if (base == 0) {
        return 0;
    }
    //转换成内存elf
    MemoryElf *elf = memory_elf_create((void *) (base), LINKER_PATH);
    if (elf != NULL) {
        //回调Linker
        int result = dl_iterate_phdr_callback_memory_elf(elf, callback, data);
        memory_elf_destroy(elf);
        return result;
    }
    memory_elf_destroy(elf);
    return 0;
}

static int dl_iterate_phdr_by_system_callback(struct dl_phdr_info *info, size_t size, void *dataArr) {
    uintptr_t *params = (uintptr_t *) (dataArr);
    dl_iterate_phdr_cb_t callback = (dl_iterate_phdr_cb_t) (params[0]);
    void *data = (void *) (params[1]);
    MapsVisitor_t *mapVisitor = (MapsVisitor_t *) (params[2]);

    /**
     * 在Android 7.1及之前版本，libdl.so是第一个返回的info，并且它的信息是代码构造出来的，
     * libdl.so并不真正被加载到内存中，此时info->dlpi_name为NULL。
     * 因此在这种情况下，忽略该回调。
     */
    if (info->dlpi_addr == 0 && info->dlpi_name == NULL) {
        return 0;
    }

    /**
     * 在Android 7.1及之前版本，原生dl_iterate_phdr不会遍历到linker/linker64.
     * 因此dl_iterate_phdr_linker_single()方法会遍历一次linker，这里是防止遍历两次linker
     */
    if (info->dlpi_addr == linkerBase()) {
        LOGV(TAG, "linkerAddress = %#" ADDRESS, linkerBase());
        return 0;
    }

    /**
     * 1、Android 5.1及之前的设备，dl_iterate_phdr只能返回ELF的basename，而不是pathname。
     * 2、Android 7.1及之后的设备，dl_iterate_phdr对于当前进程，会返回包名，这里也替换成app_process(64)路径
     */
    if (info->dlpi_name[0] != '/' && info->dlpi_name[0] != '[') {
        uintptr_t base = (uintptr_t) (get_ehdr_from_dl_phdr_info(info));
        //重置文件流，避免打开多个maps文件导致fd溢出OOM
        maps_visitor_reset(mapVisitor);
        while (maps_visitor_has_next(mapVisitor)) {
            MapItem mapItem = {0};
            if (maps_visitor_next(mapVisitor, &mapItem) == NULL) {
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
    }

    return callback(info, size, data);
}

static int dl_iterate_phdr_by_system(dl_iterate_phdr_cb_t callback, void *data) {
    //找不到dl_iterate_phdr方法，返回0表示结束
    if (dl_iterate_phdr == NULL) {
        LOGW(TAG, "dl_iterate_phdr_by_system() dl_iterate_phdr is null");
        return 0;
    }

    MapsVisitor_t *mapVisitor = maps_visitor_create(0);
    //这里把mapVisitor作为参数，避免打开多个maps文件导致fd溢出OOM
    if (!maps_visitor_valid(mapVisitor)) {
        maps_visitor_destroy(mapVisitor);
        LOGE(TAG, "dl_iterate_phdr_by_system() open maps visitor fail");
        return 0;
    }
    uintptr_t params[] = {(uintptr_t) callback, (uintptr_t) data, (uintptr_t) mapVisitor};
    const int apiLevel = android_get_device_api_level();
    /**
     * Android 5.1及之前版本，dl_iterate_phdr 的实现不持linker全局锁，
     * 需要自己找linker的符号（__dl__ZL10g_dl_mutex）自己加锁
     */
    if (apiLevel < __ANDROID_API_M__) {
        lockLinker();
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

    result = dl_iterate_phdr(dl_iterate_phdr_by_system_callback, params);
    if (apiLevel < __ANDROID_API_M__) {
        unLockLinker();
    }
    maps_visitor_destroy(mapVisitor);
    return result;
}


int dl_iterate_phdr_enhance(dl_iterate_phdr_cb_t callback, void *data) {
#if defined(__arm__) && __ANDROID_API__ < __ANDROID_API_L__
    //如果是arm平台并且运行时API版本低于API21，那么使用maps查找的方式
    if(android_get_device_api_level() < __ANDROID_API_L__ || dl_iterate_phdr == NULL){
        return dl_iterate_phdr_by_maps(callback, data);
    }else{
        //否则，支持原生dl_iterate_phdr
        return dl_iterate_phdr_by_system(callback, data);
    }
#else
    //其他，使用原生支持dl_iterate_phdr
    return dl_iterate_phdr_by_system(callback, data);
#endif
}