#include "hook_linker.h"
#include "disk_elf.h"
#include "memory_elf.h"
#include "Log.h"
#include <pthread.h>
#include <sys/auxv.h>

#define TAG "dlfcn_bypass"
/**
 * 见源码http://aospxref.com/android-5.1.1_r9/xref/bionic/linker/dlfcn.cpp#32。
 * static pthread_mutex_t g_dl_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
 * 即本地符号g_dl_mutex
 */
#define XDL_LINKER_SYM_G_DL_MUTEX           "__dl__ZL10g_dl_mutex"
//linker_mutex从linker内存通过符号查找的方式找到，linker_mutex是一个可重入锁
static pthread_mutex_t *linker_mutex = NULL;

static void initLinkerMutex() {
    int apiLevel = android_get_device_api_level();
    //Android 5.x
    if (apiLevel == __ANDROID_API_L__ || apiLevel == __ANDROID_API_L_MR1__
        || apiLevel == __ANDROID_API_N__ || apiLevel == __ANDROID_API_N_MR1__) {
        if (getauxval == NULL) {
            return;
        }
        uintptr_t base = getauxval(AT_BASE);
        if (base == 0) {
            return;
        }
        MemoryElf_t *memoryElf = memory_elf_create_by_handle((void *) base, LINKER_PATH);
        if (!memory_elf_is_valid(memoryElf)) {
            memory_elf_destroy(memoryElf);
            return;
        }
        ElfW(Sym) *symbol = memory_elf_look_up_symbol(memoryElf, XDL_LINKER_SYM_G_DL_MUTEX, NULL);
        if (symbol == NULL) {
            memory_elf_destroy(memoryElf);
            return;
        }
//        DiskElf_t *diskElf = disk_elf_create_by_file_path(LINKER_PATH);
//        if (!disk_elf_is_valid(diskElf)) {
//            memory_elf_destroy(memoryElf);
//            disk_elf_destroy(diskElf);
//            return;
//        }
//        //根据elf文件，找到XDL_LINKER_SYM_G_DL_MUTEX符号的偏移量
//        ElfW(Sym) *symbol = disk_elf_look_up_symbol(diskElf, XDL_LINKER_SYM_G_DL_MUTEX, NULL);

        //根据内存load_bias,计算变量的地址
        linker_mutex = (pthread_mutex_t *) (memory_elf_get_load_bias(memoryElf) + symbol->st_value);
        memory_elf_destroy(memoryElf);
//        disk_elf_destroy(diskElf);
    }
}

//保证只初始化一次
static pthread_once_t once = PTHREAD_ONCE_INIT;

void lockLinker() {
    pthread_once(&once, initLinkerMutex);
    if (linker_mutex != NULL && pthread_mutex_lock(linker_mutex) == 0) {
        LOGE(TAG, "lockLinker success");
    } else {
        LOGE(TAG, "lockLinker fail");
    }
}

void unLockLinker() {
    if (linker_mutex != NULL && pthread_mutex_unlock(linker_mutex) == 0) {
        LOGE(TAG, "unLockLinker success");
    } else {
        LOGE(TAG, "unLockLinker fail");
    }
}