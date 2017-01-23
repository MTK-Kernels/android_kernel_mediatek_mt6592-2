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

#ifndef GLCONTEXT_H_
#define GLCONTEXT_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>

#include "JNIHelper.h"

namespace ndk_helper
{



class GLContext
{
private:
    
    ANativeWindow* window_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLConfig config_;

    
    int32_t screen_width_;
    int32_t screen_height_;
    int32_t color_size_;
    int32_t depth_size_;

    
    bool gles_initialized_;
    bool egl_context_initialized_;
    bool es3_supported_;
    float gl_version_;
    bool context_valid_;

    void InitGLES();
    void Terminate();
    bool InitEGLSurface();
    bool InitEGLContext();

    GLContext( GLContext const& );
    void operator=( GLContext const& );
    GLContext();
    virtual ~GLContext();
public:
    static GLContext* GetInstance()
    {
        
        static GLContext instance;

        return &instance;
    }

    bool Init( ANativeWindow* window );
    EGLint Swap();
    bool Invalidate();

    void Suspend();
    EGLint Resume( ANativeWindow* window );

    int32_t GetScreenWidth()
    {
        return screen_width_;
    }
    int32_t GetScreenHeight()
    {
        return screen_height_;
    }

    int32_t GetBufferColorSize()
    {
        return color_size_;
    }
    int32_t GetBufferDepthSize()
    {
        return depth_size_;
    }
    float GetGLVersion()
    {
        return gl_version_;
    }
    bool CheckExtension( const char* extension );
};

}   

#endif 
