/* Copyright (c) 2025 ThePedroo. All rights reserved.
 *
 * This source code is licensed under the GNU AGPLv3 License found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#define LOG_TAG "CSOLoader"

#ifdef __ANDROID__
  #include <android/log.h>

  #ifdef CSOLOADER_DEBUG
    #ifdef __ANDROID__
      #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
      #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      #define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
      #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
      #define LOGF(...) do { __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__); abort(); } while(0)
      #define PLOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt ": %s", ##__VA_ARGS__, strerror(errno))
    #else
      #define LOGD(...) fprintf(stderr, "[D] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
      #define LOGI(...) fprintf(stderr, "[I] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
      #define LOGW(...) fprintf(stderr, "[W] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
      #define LOGE(...) fprintf(stderr, "[E] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
      #define LOGF(...) do { fprintf(stderr, "[F] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while(0)
      #define PLOGE(fmt, ...) fprintf(stderr, "[E] " LOG_TAG ": " fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
    #endif
  #else
    #define LOGD(...) do {} while(0)
    #define LOGI(...) do {} while(0)
    #define LOGW(...) do {} while(0)
    #define LOGE(...) do {} while(0)
    #define LOGF(...) do { abort(); } while(0)
    #define PLOGE(fmt, ...) do {} while(0)
  #endif
#else
  #ifdef CSOLOADER_DEBUG
    #define LOGD(...) fprintf(stderr, "[D] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
    #define LOGI(...) fprintf(stderr, "[I] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
    #define LOGW(...) fprintf(stderr, "[W] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
    #define LOGE(...) fprintf(stderr, "[E] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n")
    #define LOGF(...) do { fprintf(stderr, "[F] " LOG_TAG ": " __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while(0)
    #define PLOGE(fmt, ...) fprintf(stderr, "[E] " LOG_TAG ": " fmt ": %s\n", ##__VA_ARGS__, strerror(errno))
  #else
    #define LOGD(...) do {} while(0)
    #define LOGI(...) do {} while(0)
    #define LOGW(...) do {} while(0)
    #define LOGE(...) do {} while(0)
    #define LOGF(...) do { abort(); } while(0)
    #define PLOGE(fmt, ...) do {} while(0)
  #endif
#endif

#endif /* LOGGING_H */