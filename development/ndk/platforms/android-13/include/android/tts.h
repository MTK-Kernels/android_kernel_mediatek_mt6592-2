/*
 * Copyright (C) 2009 Google Inc.
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
#ifndef ANDROID_TTS_H
#define ANDROID_TTS_H 



#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ANDROID_TTS_ENGINE_PROPERTY_CONFIG "engineConfig"
#define ANDROID_TTS_ENGINE_PROPERTY_PITCH  "pitch"
#define ANDROID_TTS_ENGINE_PROPERTY_RATE   "rate"
#define ANDROID_TTS_ENGINE_PROPERTY_VOLUME "volume"

typedef enum {
    ANDROID_TTS_SUCCESS                 = 0,
    ANDROID_TTS_FAILURE                 = -1,
    ANDROID_TTS_FEATURE_UNSUPPORTED     = -2,
    ANDROID_TTS_VALUE_INVALID           = -3,
    ANDROID_TTS_PROPERTY_UNSUPPORTED    = -4,
    ANDROID_TTS_PROPERTY_SIZE_TOO_SMALL = -5,
    ANDROID_TTS_MISSING_RESOURCES       = -6
} android_tts_result_t;

typedef enum {
    ANDROID_TTS_LANG_COUNTRY_VAR_AVAILABLE = 2,
    ANDROID_TTS_LANG_COUNTRY_AVAILABLE    = 1,
    ANDROID_TTS_LANG_AVAILABLE            = 0,
    ANDROID_TTS_LANG_MISSING_DATA         = -1,
    ANDROID_TTS_LANG_NOT_SUPPORTED        = -2
} android_tts_support_result_t;

typedef enum {
    ANDROID_TTS_SYNTH_DONE              = 0,
    ANDROID_TTS_SYNTH_PENDING           = 1
} android_tts_synth_status_t;

typedef enum {
    ANDROID_TTS_CALLBACK_HALT           = 0,
    ANDROID_TTS_CALLBACK_CONTINUE       = 1
} android_tts_callback_status_t;

typedef enum {
    ANDROID_TTS_AUDIO_FORMAT_INVALID    = -1,
    ANDROID_TTS_AUDIO_FORMAT_DEFAULT    = 0,
    ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT = 1,
    ANDROID_TTS_AUDIO_FORMAT_PCM_8_BIT  = 2,
} android_tts_audio_format_t;


typedef struct android_tts_engine_funcs_t  android_tts_engine_funcs_t;

typedef struct {
    android_tts_engine_funcs_t *funcs;
} android_tts_engine_t;

extern android_tts_engine_t *android_getTtsEngine();

android_tts_engine_t *getTtsEngine();

typedef android_tts_callback_status_t (*android_tts_synth_cb_t)
            (void **pUserData,
             uint32_t trackSamplingHz,
             android_tts_audio_format_t audioFormat,
             int channelCount,
             int8_t **pAudioBuffer,
             size_t *pBufferSize,
             android_tts_synth_status_t status);



struct android_tts_engine_funcs_t {
    
    
    
    void *reserved[2];

    
    
    
    android_tts_result_t (*init)
            (void *engine,
             android_tts_synth_cb_t synthDonePtr,
             const char *engineConfig);

    
    
    android_tts_result_t (*shutdown)
            (void *engine);

    
    
    
    android_tts_result_t (*stop)
            (void *engine);

    
    
    
    
    
    
    
    
    
    
    
    
    android_tts_support_result_t (*isLanguageAvailable)
            (void *engine,
             const char *lang,
             const char *country,
             const char *variant);

    
    
    
    
    
    
    
    
    
    
    android_tts_result_t (*loadLanguage)
            (void *engine,
             const char *lang,
             const char *country,
             const char *variant);

    
    
    
    
    
    
    
    
    
    
    android_tts_result_t (*setLanguage)
            (void *engine,
             const char *lang,
             const char *country,
             const char *variant);

    
    
    
    
    
    
    android_tts_result_t (*getLanguage)
            (void *engine,
             char *language,
             char *country,
             char *variant);

    
    
    
    
    
    
    
    
    
    
    android_tts_result_t (*setAudioFormat)
            (void *engine,
             android_tts_audio_format_t* pEncoding,
             uint32_t* pRate,
             int* pChannels);

    
    
    
    
    
    
    
    android_tts_result_t (*setProperty)
            (void *engine,
             const char *property,
             const char *value,
             const size_t size);

    
    
    
    
    
    
    
    
    
    
    android_tts_result_t (*getProperty)
            (void *engine,
             const char *property,
             char *value,
             size_t *iosize);

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    // @param buffer    the location where the synthesized data must be written
    // @param bufferSize the number of bytes that can be written in buffer
    
    android_tts_result_t (*synthesizeText)
            (void *engine,
             const char *text,
             int8_t *buffer,
             size_t bufferSize,
             void *userdata);
};

#ifdef __cplusplus
}
#endif

#endif 
