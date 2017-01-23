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

#ifndef OPENSL_ES_ANDROIDCONFIGURATION_H_
#define OPENSL_ES_ANDROIDCONFIGURATION_H_

#ifdef __cplusplus
extern "C" {
#endif


#define SL_ANDROID_KEY_RECORDING_PRESET ((const SLchar*) "androidRecordingPreset")
#define SL_ANDROID_RECORDING_PRESET_NONE              ((SLuint32) 0x00000000)
#define SL_ANDROID_RECORDING_PRESET_GENERIC           ((SLuint32) 0x00000001)
#define SL_ANDROID_RECORDING_PRESET_CAMCORDER         ((SLuint32) 0x00000002)
#define SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION ((SLuint32) 0x00000003)


#define SL_ANDROID_KEY_STREAM_TYPE ((const SLchar*) "androidPlaybackStreamType")

#define SL_ANDROID_STREAM_VOICE        ((SLint32) 0x00000000)
#define SL_ANDROID_STREAM_SYSTEM       ((SLint32) 0x00000001)
#define SL_ANDROID_STREAM_RING         ((SLint32) 0x00000002)
#define SL_ANDROID_STREAM_MEDIA        ((SLint32) 0x00000003)
#define SL_ANDROID_STREAM_ALARM        ((SLint32) 0x00000004)
#define SL_ANDROID_STREAM_NOTIFICATION ((SLint32) 0x00000005)



#ifdef __cplusplus
}
#endif 

#endif 
