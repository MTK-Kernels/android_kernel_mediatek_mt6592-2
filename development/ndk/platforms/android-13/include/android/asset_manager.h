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


#ifndef ANDROID_ASSET_MANAGER_H
#define ANDROID_ASSET_MANAGER_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AAssetManager;
typedef struct AAssetManager AAssetManager;

struct AAssetDir;
typedef struct AAssetDir AAssetDir;

struct AAsset;
typedef struct AAsset AAsset;

enum {
    AASSET_MODE_UNKNOWN      = 0,
    AASSET_MODE_RANDOM       = 1,
    AASSET_MODE_STREAMING    = 2,
    AASSET_MODE_BUFFER       = 3
};


AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName);

AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int mode);

const char* AAssetDir_getNextFileName(AAssetDir* assetDir);

void AAssetDir_rewind(AAssetDir* assetDir);

void AAssetDir_close(AAssetDir* assetDir);

int AAsset_read(AAsset* asset, void* buf, size_t count);

off_t AAsset_seek(AAsset* asset, off_t offset, int whence);

off64_t AAsset_seek64(AAsset* asset, off64_t offset, int whence);

void AAsset_close(AAsset* asset);

const void* AAsset_getBuffer(AAsset* asset);

off_t AAsset_getLength(AAsset* asset);

off64_t AAsset_getLength64(AAsset* asset);

off_t AAsset_getRemainingLength(AAsset* asset);

off64_t AAsset_getRemainingLength64(AAsset* asset);

int AAsset_openFileDescriptor(AAsset* asset, off_t* outStart, off_t* outLength);

int AAsset_openFileDescriptor64(AAsset* asset, off64_t* outStart, off64_t* outLength);

int AAsset_isAllocated(AAsset* asset);



#ifdef __cplusplus
};
#endif

#endif      
