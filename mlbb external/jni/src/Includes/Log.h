#ifndef LOG_H
#define LOG_H

#include <android/log.h>

#ifndef OBFUSCATE
#define OBFUSCATE(x) x
#endif

#ifndef TAG
#define TAG "Vulkan"
#endif

#ifndef LOG_TAG
#define LOG_TAG OBFUSCATE("plzs")
#endif

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#endif
