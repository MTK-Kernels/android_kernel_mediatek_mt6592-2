/*
 * Copyright (C) 2011 The Android Open Source Project
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


#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <android/log.h>
#define TAG "NativeMedia"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)

#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h>

#include <android/native_window_jni.h>

static XAObjectItf engineObject = NULL;
static XAEngineItf engineEngine = NULL;

static XAObjectItf outputMixObject = NULL;

static XAObjectItf             playerObj = NULL;
static XAPlayItf               playerPlayItf = NULL;
static XAAndroidBufferQueueItf playerBQItf = NULL;
static XAStreamInformationItf  playerStreamInfoItf = NULL;
static XAVolumeItf             playerVolItf = NULL;

#define NB_MAXAL_INTERFACES 3 

static ANativeWindow* theNativeWindow;

#define NB_BUFFERS 8

#define MPEG2_TS_PACKET_SIZE 188

#define PACKETS_PER_BUFFER 10

#define BUFFER_SIZE (PACKETS_PER_BUFFER*MPEG2_TS_PACKET_SIZE)

static char dataCache[BUFFER_SIZE * NB_BUFFERS];

static FILE *file;

static jboolean reachedEof = JNI_FALSE;

static const int kEosBufferCntxt = 1980; 


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static jboolean discontinuity = JNI_FALSE;

static jboolean enqueueInitialBuffers(jboolean discontinuity);

static XAresult AndroidBufferQueueCallback(
        XAAndroidBufferQueueItf caller,
        void *pCallbackContext,        
        void *pBufferContext,          
        void *pBufferData,             
        XAuint32 dataSize,             
        XAuint32 dataUsed,             
        const XAAndroidBufferItem *pItems,
        XAuint32 itemsLength           )
{
    XAresult res;
    int ok;

    
    assert(NULL == pCallbackContext);

    
    ok = pthread_mutex_lock(&mutex);
    assert(0 == ok);

    
    if (discontinuity) {
        
        
        if (!reachedEof) {
            
            res = (*playerBQItf)->Clear(playerBQItf);
            assert(XA_RESULT_SUCCESS == res);
            
            rewind(file);
            
            (void) enqueueInitialBuffers(JNI_TRUE);
        }
        
        discontinuity = JNI_FALSE;
        ok = pthread_cond_signal(&cond);
        assert(0 == ok);
        goto exit;
    }

    if ((pBufferData == NULL) && (pBufferContext != NULL)) {
        const int processedCommand = *(int *)pBufferContext;
        if (kEosBufferCntxt == processedCommand) {
            LOGV("EOS was processed\n");
            
            assert(0 == dataSize);
            goto exit;
        }
    }

    
    assert((dataSize > 0) && ((dataSize % MPEG2_TS_PACKET_SIZE) == 0));
    assert(dataCache <= (char *) pBufferData && (char *) pBufferData <
            &dataCache[BUFFER_SIZE * NB_BUFFERS]);
    assert(0 == (((char *) pBufferData - dataCache) % BUFFER_SIZE));

    
    if (reachedEof) {
        goto exit;
    }

    size_t nbRead;
    
    size_t bytesRead;
    bytesRead = fread(pBufferData, 1, BUFFER_SIZE, file);
    if (bytesRead > 0) {
        if ((bytesRead % MPEG2_TS_PACKET_SIZE) != 0) {
            LOGV("Dropping last packet because it is not whole");
        }
        size_t packetsRead = bytesRead / MPEG2_TS_PACKET_SIZE;
        size_t bufferSize = packetsRead * MPEG2_TS_PACKET_SIZE;
        res = (*caller)->Enqueue(caller, NULL ,
                pBufferData ,
                bufferSize ,
                NULL ,
                0 );
        assert(XA_RESULT_SUCCESS == res);
    } else {
        
        XAAndroidBufferItem msgEos[1];
        msgEos[0].itemKey = XA_ANDROID_ITEMKEY_EOS;
        msgEos[0].itemSize = 0;
        
        
        res = (*caller)->Enqueue(caller, (void *)&kEosBufferCntxt ,
                NULL , 0 ,
                msgEos ,
                sizeof(XAuint32)*2 );
        assert(XA_RESULT_SUCCESS == res);
        reachedEof = JNI_TRUE;
    }

exit:
    ok = pthread_mutex_unlock(&mutex);
    assert(0 == ok);
    return XA_RESULT_SUCCESS;
}


static void StreamChangeCallback(XAStreamInformationItf caller,
        XAuint32 eventId,
        XAuint32 streamIndex,
        void * pEventData,
        void * pContext )
{
    LOGV("StreamChangeCallback called for stream %u", streamIndex);
    
    assert(NULL == pContext);
    switch (eventId) {
      case XA_STREAMCBEVENT_PROPERTYCHANGE: {

        XAresult res;
        XAuint32 domain;
        res = (*caller)->QueryStreamType(caller, streamIndex, &domain);
        assert(XA_RESULT_SUCCESS == res);
        switch (domain) {
          case XA_DOMAINTYPE_VIDEO: {
            XAVideoStreamInformation videoInfo;
            res = (*caller)->QueryStreamInformation(caller, streamIndex, &videoInfo);
            assert(XA_RESULT_SUCCESS == res);
            LOGV("Found video size %u x %u, codec ID=%u, frameRate=%u, bitRate=%u, duration=%u ms",
                        videoInfo.width, videoInfo.height, videoInfo.codecId, videoInfo.frameRate,
                        videoInfo.bitRate, videoInfo.duration);
          } break;
          default:
            fprintf(stderr, "Unexpected domain %u\n", domain);
            break;
        }
      } break;
      default:
        fprintf(stderr, "Unexpected stream event ID %u\n", eventId);
        break;
    }
}


void Java_com_example_nativemedia_NativeMedia_createEngine(JNIEnv* env, jclass clazz)
{
    XAresult res;

    
    res = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*outputMixObject)->Realize(outputMixObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

}


static jboolean enqueueInitialBuffers(jboolean discontinuity)
{

    size_t bytesRead;
    bytesRead = fread(dataCache, 1, BUFFER_SIZE * NB_BUFFERS, file);
    if (bytesRead <= 0) {
        
        return JNI_FALSE;
    }
    if ((bytesRead % MPEG2_TS_PACKET_SIZE) != 0) {
        LOGV("Dropping last packet because it is not whole");
    }
    size_t packetsRead = bytesRead / MPEG2_TS_PACKET_SIZE;
    LOGV("Initially queueing %zu packets", packetsRead);

    size_t i;
    for (i = 0; i < NB_BUFFERS && packetsRead > 0; i++) {
        
        size_t packetsThisBuffer = packetsRead;
        if (packetsThisBuffer > PACKETS_PER_BUFFER) {
            packetsThisBuffer = PACKETS_PER_BUFFER;
        }
        size_t bufferSize = packetsThisBuffer * MPEG2_TS_PACKET_SIZE;
        XAresult res;
        if (discontinuity) {
            
            XAAndroidBufferItem items[1];
            items[0].itemKey = XA_ANDROID_ITEMKEY_DISCONTINUITY;
            items[0].itemSize = 0;
            
            
            
            res = (*playerBQItf)->Enqueue(playerBQItf, NULL ,
                    dataCache + i*BUFFER_SIZE, bufferSize, items ,
                    sizeof(XAuint32)*2 );
            discontinuity = JNI_FALSE;
        } else {
            res = (*playerBQItf)->Enqueue(playerBQItf, NULL ,
                    dataCache + i*BUFFER_SIZE, bufferSize, NULL, 0);
        }
        assert(XA_RESULT_SUCCESS == res);
        packetsRead -= packetsThisBuffer;
    }

    return JNI_TRUE;
}


jboolean Java_com_example_nativemedia_NativeMedia_createStreamingMediaPlayer(JNIEnv* env,
        jclass clazz, jstring filename)
{
    XAresult res;

    
    const char *utf8 = (*env)->GetStringUTFChars(env, filename, NULL);
    assert(NULL != utf8);

    
    file = fopen(utf8, "rb");
    if (file == NULL) {
        return JNI_FALSE;
    }

    
    XADataLocator_AndroidBufferQueue loc_abq = { XA_DATALOCATOR_ANDROIDBUFFERQUEUE, NB_BUFFERS };
    XADataFormat_MIME format_mime = {
            XA_DATAFORMAT_MIME, XA_ANDROID_MIME_MP2TS, XA_CONTAINERTYPE_MPEG_TS };
    XADataSource dataSrc = {&loc_abq, &format_mime};

    
    XADataLocator_OutputMix loc_outmix = { XA_DATALOCATOR_OUTPUTMIX, outputMixObject };
    XADataSink audioSnk = { &loc_outmix, NULL };

    
    XADataLocator_NativeDisplay loc_nd = {
            XA_DATALOCATOR_NATIVEDISPLAY,        
            
            (void*)theNativeWindow,              
            
            NULL                                 
    };
    XADataSink imageVideoSink = {&loc_nd, NULL};

    
    XAboolean     required[NB_MAXAL_INTERFACES]
                           = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE,           XA_BOOLEAN_TRUE};
    XAInterfaceID iidArray[NB_MAXAL_INTERFACES]
                           = {XA_IID_PLAY,     XA_IID_ANDROIDBUFFERQUEUESOURCE,
                                               XA_IID_STREAMINFORMATION};

    
    res = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObj, &dataSrc,
            NULL, &audioSnk, &imageVideoSink, NULL, NULL,
            NB_MAXAL_INTERFACES ,
            iidArray ,
            required );
    assert(XA_RESULT_SUCCESS == res);

    
    (*env)->ReleaseStringUTFChars(env, filename, utf8);

    
    res = (*playerObj)->Realize(playerObj, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerObj)->GetInterface(playerObj, XA_IID_PLAY, &playerPlayItf);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerObj)->GetInterface(playerObj, XA_IID_STREAMINFORMATION, &playerStreamInfoItf);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerObj)->GetInterface(playerObj, XA_IID_VOLUME, &playerVolItf);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerObj)->GetInterface(playerObj, XA_IID_ANDROIDBUFFERQUEUESOURCE, &playerBQItf);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerBQItf)->SetCallbackEventsMask(playerBQItf, XA_ANDROIDBUFFERQUEUEEVENT_PROCESSED);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerBQItf)->RegisterCallback(playerBQItf, AndroidBufferQueueCallback, NULL);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerStreamInfoItf)->RegisterStreamChangeCallback(playerStreamInfoItf,
            StreamChangeCallback, NULL);
    assert(XA_RESULT_SUCCESS == res);

    
    if (!enqueueInitialBuffers(JNI_FALSE)) {
        return JNI_FALSE;
    }

    
    res = (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_PAUSED);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerVolItf)->SetVolumeLevel(playerVolItf, 0);
    assert(XA_RESULT_SUCCESS == res);

    
    res = (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_PLAYING);
        assert(XA_RESULT_SUCCESS == res);

    return JNI_TRUE;
}


void Java_com_example_nativemedia_NativeMedia_setPlayingStreamingMediaPlayer(JNIEnv* env,
        jclass clazz, jboolean isPlaying)
{
    XAresult res;

    
    if (NULL != playerPlayItf) {

        
        res = (*playerPlayItf)->SetPlayState(playerPlayItf, isPlaying ?
            XA_PLAYSTATE_PLAYING : XA_PLAYSTATE_PAUSED);
        assert(XA_RESULT_SUCCESS == res);

    }

}


void Java_com_example_nativemedia_NativeMedia_shutdown(JNIEnv* env, jclass clazz)
{
    
    if (playerObj != NULL) {
        (*playerObj)->Destroy(playerObj);
        playerObj = NULL;
        playerPlayItf = NULL;
        playerBQItf = NULL;
        playerStreamInfoItf = NULL;
        playerVolItf = NULL;
    }

    
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }

    
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

    
    if (file != NULL) {
        fclose(file);
        file = NULL;
    }

    
    if (theNativeWindow != NULL) {
        ANativeWindow_release(theNativeWindow);
        theNativeWindow = NULL;
    }
}


void Java_com_example_nativemedia_NativeMedia_setSurface(JNIEnv *env, jclass clazz, jobject surface)
{
    
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
}


void Java_com_example_nativemedia_NativeMedia_rewindStreamingMediaPlayer(JNIEnv *env, jclass clazz)
{
    XAresult res;

    
    if (NULL != playerBQItf && NULL != file) {
        
        int ok;
        ok = pthread_mutex_lock(&mutex);
        assert(0 == ok);
        discontinuity = JNI_TRUE;
        
        
        
        while (discontinuity && !reachedEof) {
            ok = pthread_cond_wait(&cond, &mutex);
            assert(0 == ok);
        }
        ok = pthread_mutex_unlock(&mutex);
        assert(0 == ok);
    }

}
