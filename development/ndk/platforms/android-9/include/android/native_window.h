/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_NATIVE_WINDOW_H
#define ANDROID_NATIVE_WINDOW_H

#include <android/rect.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    WINDOW_FORMAT_RGBA_8888          = 1,
    WINDOW_FORMAT_RGBX_8888          = 2,
    WINDOW_FORMAT_RGB_565            = 4,
};

struct ANativeWindow;
typedef struct ANativeWindow ANativeWindow;

typedef struct ANativeWindow_Buffer {
    
    int32_t width;

    
    int32_t height;

    
    
    int32_t stride;

    
    int32_t format;

    
    void* bits;
    
    
    uint32_t reserved[6];
} ANativeWindow_Buffer;

void ANativeWindow_acquire(ANativeWindow* window);

void ANativeWindow_release(ANativeWindow* window);

int32_t ANativeWindow_getWidth(ANativeWindow* window);

int32_t ANativeWindow_getHeight(ANativeWindow* window);

int32_t ANativeWindow_getFormat(ANativeWindow* window);

int32_t ANativeWindow_setBuffersGeometry(ANativeWindow* window, int32_t width, int32_t height, int32_t format);

int32_t ANativeWindow_lock(ANativeWindow* window, ANativeWindow_Buffer* outBuffer,
        ARect* inOutDirtyBounds);

int32_t ANativeWindow_unlockAndPost(ANativeWindow* window);

#ifdef __cplusplus
};
#endif

#endif 
