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


#ifndef ANDROID_SENSOR_H
#define ANDROID_SENSOR_H



#include <sys/types.h>

#include <android/looper.h>

#ifdef __cplusplus
extern "C" {
#endif



enum {
    ASENSOR_TYPE_ACCELEROMETER      = 1,
    ASENSOR_TYPE_MAGNETIC_FIELD     = 2,
    ASENSOR_TYPE_GYROSCOPE          = 4,
    ASENSOR_TYPE_LIGHT              = 5,
    ASENSOR_TYPE_PROXIMITY          = 8
};

enum {
    ASENSOR_STATUS_UNRELIABLE       = 0,
    ASENSOR_STATUS_ACCURACY_LOW     = 1,
    ASENSOR_STATUS_ACCURACY_MEDIUM  = 2,
    ASENSOR_STATUS_ACCURACY_HIGH    = 3
};


#define ASENSOR_STANDARD_GRAVITY            (9.80665f)
#define ASENSOR_MAGNETIC_FIELD_EARTH_MAX    (60.0f)
#define ASENSOR_MAGNETIC_FIELD_EARTH_MIN    (30.0f)


typedef struct ASensorVector {
    union {
        float v[3];
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float azimuth;
            float pitch;
            float roll;
        };
    };
    int8_t status;
    uint8_t reserved[3];
} ASensorVector;

typedef struct ASensorEvent {
    int32_t version; 
    int32_t sensor;
    int32_t type;
    int32_t reserved0;
    int64_t timestamp;
    union {
        float           data[16];
        ASensorVector   vector;
        ASensorVector   acceleration;
        ASensorVector   magnetic;
        float           temperature;
        float           distance;
        float           light;
        float           pressure;
    };
    int32_t reserved1[4];
} ASensorEvent;


struct ASensorManager;
typedef struct ASensorManager ASensorManager;

struct ASensorEventQueue;
typedef struct ASensorEventQueue ASensorEventQueue;

struct ASensor;
typedef struct ASensor ASensor;
typedef ASensor const* ASensorRef;
typedef ASensorRef const* ASensorList;


ASensorManager* ASensorManager_getInstance();


int ASensorManager_getSensorList(ASensorManager* manager, ASensorList* list);

ASensor const* ASensorManager_getDefaultSensor(ASensorManager* manager, int type);

ASensorEventQueue* ASensorManager_createEventQueue(ASensorManager* manager,
        ALooper* looper, int ident, ALooper_callbackFunc callback, void* data);

int ASensorManager_destroyEventQueue(ASensorManager* manager, ASensorEventQueue* queue);



int ASensorEventQueue_enableSensor(ASensorEventQueue* queue, ASensor const* sensor);

int ASensorEventQueue_disableSensor(ASensorEventQueue* queue, ASensor const* sensor);

int ASensorEventQueue_setEventRate(ASensorEventQueue* queue, ASensor const* sensor, int32_t usec);

int ASensorEventQueue_hasEvents(ASensorEventQueue* queue);

ssize_t ASensorEventQueue_getEvents(ASensorEventQueue* queue,
                ASensorEvent* events, size_t count);



const char* ASensor_getName(ASensor const* sensor);

const char* ASensor_getVendor(ASensor const* sensor);

int ASensor_getType(ASensor const* sensor);

float ASensor_getResolution(ASensor const* sensor) __NDK_FPABI__;

int ASensor_getMinDelay(ASensor const* sensor);


#ifdef __cplusplus
};
#endif

#endif 
