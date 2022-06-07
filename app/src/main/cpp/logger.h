//
// Created by mecha on 03.06.2022.
//

#pragma once

#include <android/log.h>

#define DUAL_TAG "dual"
#define DUAL_INFO(...) __android_log_print(ANDROID_LOG_INFO,DUAL_TAG,__VA_ARGS__)
#define DUAL_ERR(...) __android_log_print(ANDROID_LOG_ERROR,DUAL_TAG,__VA_ARGS__)
#define DUAL_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG,DUAL_TAG,__VA_ARGS__)
#define DUAL_WARN(...) __android_log_print(ANDROID_LOG_WARN,DUAL_TAG,__VA_ARGS__)