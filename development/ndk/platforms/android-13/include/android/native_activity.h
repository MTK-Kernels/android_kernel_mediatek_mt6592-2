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


#ifndef ANDROID_NATIVE_ACTIVITY_H
#define ANDROID_NATIVE_ACTIVITY_H

#include <stdint.h>
#include <sys/types.h>

#include <jni.h>

#include <android/asset_manager.h>
#include <android/input.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ANativeActivityCallbacks;

typedef struct ANativeActivity {
    struct ANativeActivityCallbacks* callbacks;

    JavaVM* vm;

    JNIEnv* env;

    jobject clazz;

    const char* internalDataPath;
    
    const char* externalDataPath;
    
    int32_t sdkVersion;
    
    void* instance;

    AAssetManager* assetManager;

    const char* obbPath;
} ANativeActivity;

typedef struct ANativeActivityCallbacks {
    void (*onStart)(ANativeActivity* activity);
    
    void (*onResume)(ANativeActivity* activity);
    
    void* (*onSaveInstanceState)(ANativeActivity* activity, size_t* outSize);
    
    void (*onPause)(ANativeActivity* activity);
    
    void (*onStop)(ANativeActivity* activity);
    
    void (*onDestroy)(ANativeActivity* activity);

    void (*onWindowFocusChanged)(ANativeActivity* activity, int hasFocus);
    
    void (*onNativeWindowCreated)(ANativeActivity* activity, ANativeWindow* window);

    void (*onNativeWindowResized)(ANativeActivity* activity, ANativeWindow* window);

    void (*onNativeWindowRedrawNeeded)(ANativeActivity* activity, ANativeWindow* window);

    void (*onNativeWindowDestroyed)(ANativeActivity* activity, ANativeWindow* window);
    
    void (*onInputQueueCreated)(ANativeActivity* activity, AInputQueue* queue);
    
    void (*onInputQueueDestroyed)(ANativeActivity* activity, AInputQueue* queue);

    void (*onContentRectChanged)(ANativeActivity* activity, const ARect* rect);

    void (*onConfigurationChanged)(ANativeActivity* activity);

    void (*onLowMemory)(ANativeActivity* activity);
} ANativeActivityCallbacks;

typedef void ANativeActivity_createFunc(ANativeActivity* activity,
        void* savedState, size_t savedStateSize);

extern ANativeActivity_createFunc ANativeActivity_onCreate;

void ANativeActivity_finish(ANativeActivity* activity);

void ANativeActivity_setWindowFormat(ANativeActivity* activity, int32_t format);

void ANativeActivity_setWindowFlags(ANativeActivity* activity,
        uint32_t addFlags, uint32_t removeFlags);

enum {
    ANATIVEACTIVITY_SHOW_SOFT_INPUT_IMPLICIT = 0x0001,
    ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED = 0x0002,
};

void ANativeActivity_showSoftInput(ANativeActivity* activity, uint32_t flags);

enum {
    ANATIVEACTIVITY_HIDE_SOFT_INPUT_IMPLICIT_ONLY = 0x0001,
    ANATIVEACTIVITY_HIDE_SOFT_INPUT_NOT_ALWAYS = 0x0002,
};

void ANativeActivity_hideSoftInput(ANativeActivity* activity, uint32_t flags);

#ifdef __cplusplus
};
#endif

#endif 

