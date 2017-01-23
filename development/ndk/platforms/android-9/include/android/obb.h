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


#ifndef ANDROID_OBB_H
#define ANDROID_OBB_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AObbInfo;
typedef struct AObbInfo AObbInfo;

enum {
    AOBBINFO_OVERLAY = 0x0001,
};

AObbInfo* AObbScanner_getObbInfo(const char* filename);

void AObbInfo_delete(AObbInfo* obbInfo);

const char* AObbInfo_getPackageName(AObbInfo* obbInfo);

int32_t AObbInfo_getVersion(AObbInfo* obbInfo);

int32_t AObbInfo_getFlags(AObbInfo* obbInfo);

#ifdef __cplusplus
};
#endif

#endif      
