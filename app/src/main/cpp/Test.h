//
// Created by chenkaiyi on 12/27/24.
//

#ifndef DLITERATEPHDRENHANCE_TEST_H
#define DLITERATEPHDRENHANCE_TEST_H

#include <android/log.h>

#if defined(__LP64__)
#define ADDRESS PRIx64
#else
#define ADDRESS PRIx32
#endif

#define LOGV(TAG, ...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__))
#define LOGI(TAG, ...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOGD(TAG, ...) ((void)__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))
#define LOGW(TAG, ...) ((void)__android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__))
#define LOGE(TAG, ...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

#endif //DLITERATEPHDRENHANCE_TEST_H
