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

#ifndef _ANDROID_INPUT_H
#define _ANDROID_INPUT_H



#include <stdint.h>
#include <sys/types.h>
#include <android/keycodes.h>
#include <android/looper.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    
    AKEY_STATE_UNKNOWN = -1,

    
    AKEY_STATE_UP = 0,

    
    AKEY_STATE_DOWN = 1,

    
    AKEY_STATE_VIRTUAL = 2
};

enum {
    
    AMETA_NONE = 0,

    
    AMETA_ALT_ON = 0x02,

    
    AMETA_ALT_LEFT_ON = 0x10,

    
    AMETA_ALT_RIGHT_ON = 0x20,

    
    AMETA_SHIFT_ON = 0x01,

    
    AMETA_SHIFT_LEFT_ON = 0x40,

    
    AMETA_SHIFT_RIGHT_ON = 0x80,

    
    AMETA_SYM_ON = 0x04
};

struct AInputEvent;
typedef struct AInputEvent AInputEvent;

enum {
    
    AINPUT_EVENT_TYPE_KEY = 1,

    
    AINPUT_EVENT_TYPE_MOTION = 2
};

enum {
    
    AKEY_EVENT_ACTION_DOWN = 0,

    
    AKEY_EVENT_ACTION_UP = 1,

    AKEY_EVENT_ACTION_MULTIPLE = 2
};

enum {
    
    AKEY_EVENT_FLAG_WOKE_HERE = 0x1,

    
    AKEY_EVENT_FLAG_SOFT_KEYBOARD = 0x2,

    
    AKEY_EVENT_FLAG_KEEP_TOUCH_MODE = 0x4,

    AKEY_EVENT_FLAG_FROM_SYSTEM = 0x8,

    AKEY_EVENT_FLAG_EDITOR_ACTION = 0x10,

    AKEY_EVENT_FLAG_CANCELED = 0x20,

    AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY = 0x40,

    AKEY_EVENT_FLAG_LONG_PRESS = 0x80,

    AKEY_EVENT_FLAG_CANCELED_LONG_PRESS = 0x100,

    AKEY_EVENT_FLAG_TRACKING = 0x200
};


#define AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT 8

enum {
    AMOTION_EVENT_ACTION_MASK = 0xff,

    AMOTION_EVENT_ACTION_POINTER_INDEX_MASK  = 0xff00,

    AMOTION_EVENT_ACTION_DOWN = 0,

    AMOTION_EVENT_ACTION_UP = 1,

    AMOTION_EVENT_ACTION_MOVE = 2,

    AMOTION_EVENT_ACTION_CANCEL = 3,

    AMOTION_EVENT_ACTION_OUTSIDE = 4,

    AMOTION_EVENT_ACTION_POINTER_DOWN = 5,

    AMOTION_EVENT_ACTION_POINTER_UP = 6
};

enum {
    AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED = 0x1,
};

enum {
    
    AMOTION_EVENT_EDGE_FLAG_NONE = 0,

    
    AMOTION_EVENT_EDGE_FLAG_TOP = 0x01,

    
    AMOTION_EVENT_EDGE_FLAG_BOTTOM = 0x02,

    
    AMOTION_EVENT_EDGE_FLAG_LEFT = 0x04,

    
    AMOTION_EVENT_EDGE_FLAG_RIGHT = 0x08
};

enum {
    AINPUT_SOURCE_CLASS_MASK = 0x000000ff,

    AINPUT_SOURCE_CLASS_BUTTON = 0x00000001,
    AINPUT_SOURCE_CLASS_POINTER = 0x00000002,
    AINPUT_SOURCE_CLASS_NAVIGATION = 0x00000004,
    AINPUT_SOURCE_CLASS_POSITION = 0x00000008,
};

enum {
    AINPUT_SOURCE_UNKNOWN = 0x00000000,

    AINPUT_SOURCE_KEYBOARD = 0x00000100 | AINPUT_SOURCE_CLASS_BUTTON,
    AINPUT_SOURCE_DPAD = 0x00000200 | AINPUT_SOURCE_CLASS_BUTTON,
    AINPUT_SOURCE_TOUCHSCREEN = 0x00001000 | AINPUT_SOURCE_CLASS_POINTER,
    AINPUT_SOURCE_MOUSE = 0x00002000 | AINPUT_SOURCE_CLASS_POINTER,
    AINPUT_SOURCE_TRACKBALL = 0x00010000 | AINPUT_SOURCE_CLASS_NAVIGATION,
    AINPUT_SOURCE_TOUCHPAD = 0x00100000 | AINPUT_SOURCE_CLASS_POSITION,

    AINPUT_SOURCE_ANY = 0xffffff00,
};

enum {
    AINPUT_KEYBOARD_TYPE_NONE = 0,
    AINPUT_KEYBOARD_TYPE_NON_ALPHABETIC = 1,
    AINPUT_KEYBOARD_TYPE_ALPHABETIC = 2,
};

enum {
    AINPUT_MOTION_RANGE_X = 0,
    AINPUT_MOTION_RANGE_Y = 1,
    AINPUT_MOTION_RANGE_PRESSURE = 2,
    AINPUT_MOTION_RANGE_SIZE = 3,
    AINPUT_MOTION_RANGE_TOUCH_MAJOR = 4,
    AINPUT_MOTION_RANGE_TOUCH_MINOR = 5,
    AINPUT_MOTION_RANGE_TOOL_MAJOR = 6,
    AINPUT_MOTION_RANGE_TOOL_MINOR = 7,
    AINPUT_MOTION_RANGE_ORIENTATION = 8,
};




int32_t AInputEvent_getType(const AInputEvent* event);

int32_t AInputEvent_getDeviceId(const AInputEvent* event);

int32_t AInputEvent_getSource(const AInputEvent* event);


int32_t AKeyEvent_getAction(const AInputEvent* key_event);

int32_t AKeyEvent_getFlags(const AInputEvent* key_event);

int32_t AKeyEvent_getKeyCode(const AInputEvent* key_event);

int32_t AKeyEvent_getScanCode(const AInputEvent* key_event);

int32_t AKeyEvent_getMetaState(const AInputEvent* key_event);

int32_t AKeyEvent_getRepeatCount(const AInputEvent* key_event);

int64_t AKeyEvent_getDownTime(const AInputEvent* key_event);

int64_t AKeyEvent_getEventTime(const AInputEvent* key_event);


int32_t AMotionEvent_getAction(const AInputEvent* motion_event);

int32_t AMotionEvent_getFlags(const AInputEvent* motion_event);

int32_t AMotionEvent_getMetaState(const AInputEvent* motion_event);

int32_t AMotionEvent_getEdgeFlags(const AInputEvent* motion_event);

int64_t AMotionEvent_getDownTime(const AInputEvent* motion_event);

int64_t AMotionEvent_getEventTime(const AInputEvent* motion_event);

float AMotionEvent_getXOffset(const AInputEvent* motion_event) __NDK_FPABI__;

float AMotionEvent_getYOffset(const AInputEvent* motion_event) __NDK_FPABI__;

float AMotionEvent_getXPrecision(const AInputEvent* motion_event) __NDK_FPABI__;

float AMotionEvent_getYPrecision(const AInputEvent* motion_event) __NDK_FPABI__;

size_t AMotionEvent_getPointerCount(const AInputEvent* motion_event);

int32_t AMotionEvent_getPointerId(const AInputEvent* motion_event, size_t pointer_index);

float AMotionEvent_getRawX(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getRawY(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getX(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getY(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getPressure(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getSize(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getTouchMajor(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getTouchMinor(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getToolMajor(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getToolMinor(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

float AMotionEvent_getOrientation(const AInputEvent* motion_event, size_t pointer_index) __NDK_FPABI__;

size_t AMotionEvent_getHistorySize(const AInputEvent* motion_event);

int64_t AMotionEvent_getHistoricalEventTime(const AInputEvent* motion_event,
        size_t history_index);

float AMotionEvent_getHistoricalRawX(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalRawY(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalX(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalY(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalPressure(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalSize(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalTouchMajor(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalTouchMinor(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalToolMajor(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalToolMinor(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;

float AMotionEvent_getHistoricalOrientation(const AInputEvent* motion_event, size_t pointer_index,
        size_t history_index) __NDK_FPABI__;


struct AInputQueue;
typedef struct AInputQueue AInputQueue;

void AInputQueue_attachLooper(AInputQueue* queue, ALooper* looper,
        int ident, ALooper_callbackFunc callback, void* data);

void AInputQueue_detachLooper(AInputQueue* queue);

int32_t AInputQueue_hasEvents(AInputQueue* queue);

int32_t AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent);

int32_t AInputQueue_preDispatchEvent(AInputQueue* queue, AInputEvent* event);

void AInputQueue_finishEvent(AInputQueue* queue, AInputEvent* event, int handled);

#ifdef __cplusplus
}
#endif

#endif 
