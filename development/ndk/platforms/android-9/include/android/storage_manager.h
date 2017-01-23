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


#ifndef ANDROID_STORAGE_MANAGER_H
#define ANDROID_STORAGE_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AStorageManager;
typedef struct AStorageManager AStorageManager;

enum {
    AOBB_STATE_MOUNTED = 1,

    AOBB_STATE_UNMOUNTED = 2,

    AOBB_STATE_ERROR_INTERNAL = 20,

    AOBB_STATE_ERROR_COULD_NOT_MOUNT = 21,

    AOBB_STATE_ERROR_COULD_NOT_UNMOUNT = 22,

    AOBB_STATE_ERROR_NOT_MOUNTED = 23,

    AOBB_STATE_ERROR_ALREADY_MOUNTED = 24,

    AOBB_STATE_ERROR_PERMISSION_DENIED = 25,
};

AStorageManager* AStorageManager_new();

void AStorageManager_delete(AStorageManager* mgr);

typedef void (*AStorageManager_obbCallbackFunc)(const char* filename, const int32_t state, void* data);

void AStorageManager_mountObb(AStorageManager* mgr, const char* filename, const char* key,
        AStorageManager_obbCallbackFunc cb, void* data);

void AStorageManager_unmountObb(AStorageManager* mgr, const char* filename, const int force,
        AStorageManager_obbCallbackFunc cb, void* data);

int AStorageManager_isObbMounted(AStorageManager* mgr, const char* filename);

const char* AStorageManager_getMountedObbPath(AStorageManager* mgr, const char* filename);


#ifdef __cplusplus
};
#endif

#endif      
