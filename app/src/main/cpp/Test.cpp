//
// Created by chenkaiyi on 12/27/24.
//

#include "Test.h"
#include "dl_iterate_phdr_enhance.h"
#include <jni.h>

#define TAG "TEST"

extern "C"
JNIEXPORT void JNICALL
Java_com_muye_dl_1iterate_1phdr_1enhance_DlIteratePhdrEnhanceTest_iteratePhdr(JNIEnv *env,
                                                                              jobject thiz) {
//    bypass_dl_iterate_phdr([](struct dl_phdr_info *info, size_t size, void *data) {
//        LOGD(TAG, "info->dlpi_name: %s", info->dlpi_name);
//        LOGD(TAG, "info->dlpi_addr: %p", info->dlpi_addr);
//        LOGD(TAG, "info->dlpi_phdr: %p", info->dlpi_phdr);
//        LOGD(TAG, "info->dlpi_phnum: %d", info->dlpi_phnum);
//        return 0;
//    }, nullptr);
    dl_iterate_phdr_by_maps([](struct dl_phdr_info *info, size_t size, void *data) {
        LOGD(TAG, "info->dlpi_name: %s", info->dlpi_name);
        LOGD(TAG, "info->dlpi_addr: %p", info->dlpi_addr);
        LOGD(TAG, "info->dlpi_phdr: %p", info->dlpi_phdr);
        LOGD(TAG, "info->dlpi_phnum: %d", info->dlpi_phnum);
        return 0;
    }, nullptr);

    //比较dl_iterate_phdr_by_maps和bypass_dl_iterate_phdr的结果是否完全相同
}

