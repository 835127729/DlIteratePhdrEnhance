//
// Created by chenkaiyi on 12/27/24.
//

#include "Test.h"
#include "dl_iterate_phdr_enhance.h"
#include <jni.h>
#include <map>

#define TAG "TEST"

extern "C"
JNIEXPORT void JNICALL
Java_com_muye_dl_1iterate_1phdr_1enhance_DlIteratePhdrEnhanceTest_system(JNIEnv *env, jobject thiz) {
    LOGI(TAG, "system遍历开始");
    dl_iterate_phdr([](struct dl_phdr_info *info, size_t size, void *data) {
        LOGD(TAG, "info->dlpi_name: %s", info->dlpi_name == nullptr ? "": info->dlpi_name);
        LOGD(TAG, "info->dlpi_addr: %p", info->dlpi_addr);
        LOGD(TAG, "info->dlpi_phdr: %p", info->dlpi_phdr);
        LOGD(TAG, "info->dlpi_phnum: %d", info->dlpi_phnum);
        return 0;
    }, nullptr);
    LOGI(TAG, "system遍历结束");
}


extern "C"
JNIEXPORT void JNICALL
Java_com_muye_dl_1iterate_1phdr_1enhance_DlIteratePhdrEnhanceTest_enhanced(JNIEnv *env, jobject thiz) {
    LOGI(TAG, "enhanced遍历开始");
    dl_iterate_phdr_enhance([](struct dl_phdr_info *info, size_t size, void *data) {
        LOGD(TAG, "info->dlpi_name: %s", info->dlpi_name);
        LOGD(TAG, "info->dlpi_phdr: %p", info->dlpi_phdr);
        LOGD(TAG, "info->dlpi_phnum: %d", info->dlpi_phnum);
        return 0;
    }, nullptr);
    LOGI(TAG, "enhanced遍历结束");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_muye_dl_1iterate_1phdr_1enhance_DlIteratePhdrEnhanceTest_byMaps(JNIEnv *env,jobject thiz) {
    LOGI(TAG, "byMaps遍历开始");
    dl_iterate_phdr_by_maps([](struct dl_phdr_info *info, size_t size, void *data) {
        LOGD(TAG, "info->dlpi_name: %s", info->dlpi_name);
        LOGD(TAG, "info->dlpi_addr: %p", info->dlpi_addr);
        LOGD(TAG, "info->dlpi_phdr: %p", info->dlpi_phdr);
        LOGD(TAG, "info->dlpi_phnum: %d", info->dlpi_phnum);
        return 0;
    }, nullptr);
    LOGI(TAG, "byMaps遍历结束");
}