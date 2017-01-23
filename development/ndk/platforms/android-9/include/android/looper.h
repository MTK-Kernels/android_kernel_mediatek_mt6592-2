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


#ifndef ANDROID_LOOPER_H
#define ANDROID_LOOPER_H

#ifdef __cplusplus
extern "C" {
#endif

struct ALooper;
typedef struct ALooper ALooper;

ALooper* ALooper_forThread();

enum {
    ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1<<0
};

ALooper* ALooper_prepare(int opts);

enum {
    ALOOPER_POLL_WAKE = -1,

    ALOOPER_POLL_CALLBACK = -2,

    ALOOPER_POLL_TIMEOUT = -3,

    ALOOPER_POLL_ERROR = -4,
};

void ALooper_acquire(ALooper* looper);

void ALooper_release(ALooper* looper);

enum {
    ALOOPER_EVENT_INPUT = 1 << 0,

    ALOOPER_EVENT_OUTPUT = 1 << 1,

    ALOOPER_EVENT_ERROR = 1 << 2,

    ALOOPER_EVENT_HANGUP = 1 << 3,

    ALOOPER_EVENT_INVALID = 1 << 4,
};

typedef int (*ALooper_callbackFunc)(int fd, int events, void* data);

int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);

int ALooper_pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData);

void ALooper_wake(ALooper* looper);

int ALooper_addFd(ALooper* looper, int fd, int ident, int events,
        ALooper_callbackFunc callback, void* data);

int ALooper_removeFd(ALooper* looper, int fd);

#ifdef __cplusplus
};
#endif

#endif 
