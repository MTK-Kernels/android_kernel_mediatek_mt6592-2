/*
 * Copyright 2013 The Android Open Source Project
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

#pragma once

#include <jni.h>
#include <vector>
#include <string>

#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, ndk_helper::JNIHelper::GetInstance()->GetAppName(), __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, ndk_helper::JNIHelper::GetInstance()->GetAppName(), __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, ndk_helper::JNIHelper::GetInstance()->GetAppName(), __VA_ARGS__))

namespace ndk_helper
{

class JNIHelper
{
private:
    std::string app_name_;

    ANativeActivity* activity_;
    jobject jni_helper_java_ref_;
    jclass jni_helper_java_class_;

    
    
    
    mutable pthread_mutex_t mutex_;

    jstring GetExternalFilesDirJString( JNIEnv *env );
    jclass RetrieveClass( JNIEnv *jni,
            const char* class_name );

    JNIHelper();
    ~JNIHelper();
    JNIHelper( const JNIHelper& rhs );
    JNIHelper& operator=( const JNIHelper& rhs );

public:
    static void Init( ANativeActivity* activity,
            const char* helper_class_name );

    static JNIHelper* GetInstance();

    bool ReadFile( const char* file_name,
            std::vector<uint8_t>* buffer_ref );

    uint32_t LoadTexture( const char* file_name );

    std::string ConvertString( const char* str,
            const char* encode );
    std::string GetExternalFilesDir();

    int32_t GetNativeAudioBufferSize();

    int32_t GetNativeAudioSampleRate();

    const char* GetAppName()
    {
        return app_name_.c_str();
    }

};

} 
