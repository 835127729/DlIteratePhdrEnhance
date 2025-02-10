#include "hook_linker.h"
#include "linker_elf.h"
#include "Log.h"
#include <pthread.h>
#include <sys/auxv.h>
#include <maps_visitor.h>
#include <dlfcn.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>

#define TAG "hook_linker"
/**
 * 见源码http://aospxref.com/android-5.1.1_r9/xref/bionic/linker/dlfcn.cpp#32。
 * static pthread_mutex_t g_dl_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
 * 即本地符号g_dl_mutex
 */
#define XDL_LINKER_SYM_G_DL_MUTEX           "__dl__ZL10g_dl_mutex"
#define SH_LINKER_SYM_G_DL_MUTEX_U_QPR2     "__dl_g_dl_mutex"

//linker_mutex从linker内存通过符号查找的方式找到，linker_mutex是一个可重入锁
static pthread_mutex_t my_linker_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *linker_mutex = &my_linker_mutex;

static void initLinkerMutex() {
    uintptr_t base = linkerBase();
    if(base == 0){
        return;
    }

    LinkerElf *linkerElf = linker_elf_create((void *) base, LINKER_PATH);
    if (linkerElf == NULL) {
        return;
    }
    const ElfW(Sym) * symbol = linker_elf_look_up_symbol(linkerElf, XDL_LINKER_SYM_G_DL_MUTEX);
    if(symbol == NULL){
        symbol = linker_elf_look_up_symbol(linkerElf, SH_LINKER_SYM_G_DL_MUTEX_U_QPR2);
    }
    if(symbol == NULL){
        linker_elf_destroy(linkerElf);
        return;
    }
    //根据内存load_bias,计算变量的地址
    linker_mutex = (pthread_mutex_t *) (linkerElf->memoryElf->load_bias + symbol->st_value);
    linker_elf_destroy(linkerElf);
}

//保证只初始化一次
static pthread_once_t once = PTHREAD_ONCE_INIT;
//记录linker的内存地址
static ElfW(Addr) linkerAddress = 0;

uintptr_t linkerBase(){
    if(linkerAddress != 0){
        return linkerAddress;
    }
    uintptr_t base = 0;
    if(getauxval != NULL){
        //getauxval()在__ANDROID_API_J_MR2__及以上才支持
        base = getauxval(AT_BASE);
    }

    if(base == 0){
        //不支持getauxval()，那么遍历maps尝试找到linker
        MapsVisitor_t *mapVisitor = maps_visitor_create(0);
        if(mapVisitor != NULL && maps_visitor_valid(mapVisitor)){
            MapItem mapItem;
            while(maps_visitor_has_next(mapVisitor)){
                maps_visitor_next(mapVisitor, &mapItem);
                if(strcmp(mapItem.path, LINKER_PATH) == 0) {
                    base = mapItem.start_address;
                    break;
                }
            }
        }
        maps_visitor_destroy(mapVisitor);
    }
    linkerAddress = base;
    return base;
}

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