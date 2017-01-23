/*
 * Copyright (C) 2014 The Android Open Source Project
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


#ifndef _NDK_MEDIA_DRM_H
#define _NDK_MEDIA_DRM_H

#include "NdkMediaError.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct AMediaDrm;
typedef struct AMediaDrm AMediaDrm;

typedef struct {
    const uint8_t *ptr;
    size_t length;
} AMediaDrmByteArray;

typedef AMediaDrmByteArray AMediaDrmSessionId;
typedef AMediaDrmByteArray AMediaDrmScope;
typedef AMediaDrmByteArray AMediaDrmKeySetId;
typedef AMediaDrmByteArray AMediaDrmSecureStop;


typedef enum AMediaDrmEventType {
    EVENT_PROVISION_REQUIRED = 1,

    /**
     * This event type indicates that the app needs to request keys from a license
     * server.  The request message data is obtained using AMediaDrm_getKeyRequest.
     */
    EVENT_KEY_REQUIRED = 2,

    /**
     * This event type indicates that the licensed usage duration for keys in a session
     * has expired.  The keys are no longer valid.
     */
    EVENT_KEY_EXPIRED = 3,

    EVENT_VENDOR_DEFINED = 4
} AMediaDrmEventType;

typedef void (*AMediaDrmEventListener)(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        AMediaDrmEventType eventType, int extra, const uint8_t *data, size_t dataSize);


bool AMediaDrm_isCryptoSchemeSupported(const uint8_t *uuid, const char *mimeType);

AMediaDrm* AMediaDrm_createByUUID(const uint8_t *uuid);

void AMediaDrm_release(AMediaDrm *);

media_status_t AMediaDrm_setOnEventListener(AMediaDrm *, AMediaDrmEventListener listener);

media_status_t AMediaDrm_openSession(AMediaDrm *, AMediaDrmSessionId *sessionId);

media_status_t AMediaDrm_closeSession(AMediaDrm *, const AMediaDrmSessionId *sessionId);

typedef enum AMediaDrmKeyType {
    KEY_TYPE_STREAMING = 1,

    KEY_TYPE_OFFLINE = 2,

    KEY_TYPE_RELEASE = 3
} AMediaDrmKeyType;

typedef struct AMediaDrmKeyValuePair {
    const char *mKey;
    const char *mValue;
} AMediaDrmKeyValue;

/**
 * A key request/response exchange occurs between the app and a license server
 * to obtain or release keys used to decrypt encrypted content.
 * AMediaDrm_getKeyRequest is used to obtain an opaque key request byte array that
 * is delivered to the license server.  The opaque key request byte array is
 * returned in KeyRequest.data.  The recommended URL to deliver the key request to
 * is returned in KeyRequest.defaultUrl.
 *
 * After the app has received the key request response from the server,
 * it should deliver to the response to the DRM engine plugin using the method
 * AMediaDrm_provideKeyResponse.
 *
 * scope may be a sessionId or a keySetId, depending on the specified keyType.
 * When the keyType is KEY_TYPE_STREAMING or KEY_TYPE_OFFLINE, scope should be set
 * to the sessionId the keys will be provided to.  When the keyType is
 * KEY_TYPE_RELEASE, scope should be set to the keySetId of the keys being released.
 * Releasing keys from a device invalidates them for all sessions.
 *
 * init container-specific data, its meaning is interpreted based on the mime type
 * provided in the mimeType parameter.  It could contain, for example, the content
 * ID, key ID or other data obtained from the content metadata that is required in
 * generating the key request. init may be null when keyType is KEY_TYPE_RELEASE.
 *
 * initSize is the number of bytes of initData
 *
 * mimeType identifies the mime type of the content.
 *
 * keyType specifes the type of the request. The request may be to acquire keys for
 *   streaming or offline content, or to release previously acquired keys, which are
 *   identified by a keySetId.
 *
 * optionalParameters are included in the key request message to allow a client
 *   application to provide additional message parameters to the server.
 *
 * numOptionalParameters indicates the number of optional parameters provided
 *   by the caller
 *
 * On exit:
 *   1. The keyRequest pointer will reference the opaque key request data.  It
 *       will reside in memory owned by the AMediaDrm object, and will remain
 *       accessible until the next call to AMediaDrm_getKeyRequest or until the
 *       MediaDrm object is released.
 *   2. keyRequestSize will be set to the size of the request
 *
 * returns MEDIADRM_NOT_PROVISIONED_ERROR if reprovisioning is needed, due to a
 * problem with the device certificate.
*/
media_status_t AMediaDrm_getKeyRequest(AMediaDrm *, const AMediaDrmScope *scope,
        const uint8_t *init, size_t initSize, const char *mimeType, AMediaDrmKeyType keyType,
        const AMediaDrmKeyValue *optionalParameters, size_t numOptionalParameters,
        const uint8_t **keyRequest, size_t *keyRequestSize);

/**
 * A key response is received from the license server by the app, then it is
 * provided to the DRM engine plugin using provideKeyResponse.  When the
 * response is for an offline key request, a keySetId is returned that can be
 * used to later restore the keys to a new session with AMediaDrm_restoreKeys.
 * When the response is for a streaming or release request, a null keySetId is
 * returned.
 *
 * scope may be a sessionId or keySetId depending on the type of the
 * response.  Scope should be set to the sessionId when the response is for either
 * streaming or offline key requests.  Scope should be set to the keySetId when
 * the response is for a release request.
 *
 * response points to the opaque response from the server
 * responseSize should be set to the size of the response in bytes
 */

media_status_t AMediaDrm_provideKeyResponse(AMediaDrm *, const AMediaDrmScope *scope,
        const uint8_t *response, size_t responseSize, AMediaDrmKeySetId *keySetId);

media_status_t AMediaDrm_restoreKeys(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        const AMediaDrmKeySetId *keySetId);

media_status_t AMediaDrm_removeKeys(AMediaDrm *, const AMediaDrmSessionId *keySetId);

/**
 * Request an informative description of the key status for the session.  The status is
 * in the form of {key, value} pairs.  Since DRM license policies vary by vendor,
 * the specific status field names are determined by each DRM vendor.  Refer to your
 * DRM provider documentation for definitions of the field names for a particular
 * DRM engine plugin.
 *
 * On entry, numPairs should be set by the caller to the maximum number of pairs
 * that can be returned (the size of the array).  On exit, numPairs will be set
 * to the number of entries written to the array.  If the number of {key, value} pairs
 * to be returned is greater than *numPairs, MEDIADRM_SHORT_BUFFER will be returned
 * and numPairs will be set to the number of pairs available.
 */
media_status_t AMediaDrm_queryKeyStatus(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        AMediaDrmKeyValue *keyValuePairs, size_t *numPairs);


media_status_t AMediaDrm_getProvisionRequest(AMediaDrm *, const uint8_t **provisionRequest,
        size_t *provisionRequestSize, const char **serverUrl);


media_status_t AMediaDrm_provideProvisionResponse(AMediaDrm *,
        const uint8_t *response, size_t responseSize);


/**
 * A means of enforcing limits on the number of concurrent streams per subscriber
 * across devices is provided via SecureStop. This is achieved by securely
 * monitoring the lifetime of sessions.
 *
 * Information from the server related to the current playback session is written
 * to persistent storage on the device when each MediaCrypto object is created.
 *
 * In the normal case, playback will be completed, the session destroyed and the
 * Secure Stops will be queried. The app queries secure stops and forwards the
 * secure stop message to the server which verifies the signature and notifies the
 * server side database that the session destruction has been confirmed. The persisted
 * record on the client is only removed after positive confirmation that the server
 * received the message using releaseSecureStops().
 *
 * numSecureStops is set by the caller to the maximum number of secure stops to
 * return.  On exit, *numSecureStops will be set to the number actually returned.
 * If *numSecureStops is too small for the number of secure stops available,
 * MEDIADRM_SHORT_BUFFER will be returned and *numSecureStops will be set to the
 * number required.
 */
media_status_t AMediaDrm_getSecureStops(AMediaDrm *,
        AMediaDrmSecureStop *secureStops, size_t *numSecureStops);

media_status_t AMediaDrm_releaseSecureStops(AMediaDrm *,
        const AMediaDrmSecureStop *ssRelease);

#define PROPERTY_VENDOR "vendor"

#define PROPERTY_VERSION "version"

#define PROPERTY_DESCRIPTION "description"

#define PROPERTY_ALGORITHMS "algorithms"

media_status_t AMediaDrm_getPropertyString(AMediaDrm *, const char *propertyName,
        const char **propertyValue);

#define PROPERTY_DEVICE_UNIQUE_ID "deviceUniqueId"

media_status_t AMediaDrm_getPropertyByteArray(AMediaDrm *, const char *propertyName,
        AMediaDrmByteArray *propertyValue);

media_status_t AMediaDrm_setPropertyString(AMediaDrm *, const char *propertyName,
        const char *value);

media_status_t AMediaDrm_setPropertyByteArray(AMediaDrm *, const char *propertyName,
        const uint8_t *value, size_t valueSize);


media_status_t AMediaDrm_encrypt(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        const char *cipherAlgorithm, uint8_t *keyId, uint8_t *iv,
        const uint8_t *input, uint8_t *output, size_t dataSize);

media_status_t AMediaDrm_decrypt(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        const char *cipherAlgorithm, uint8_t *keyId, uint8_t *iv,
        const uint8_t *input, uint8_t *output, size_t dataSize);

media_status_t AMediaDrm_sign(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        const char *macAlgorithm, uint8_t *keyId, uint8_t *message, size_t messageSize,
        uint8_t *signature, size_t *signatureSize);

media_status_t AMediaDrm_verify(AMediaDrm *, const AMediaDrmSessionId *sessionId,
        const char *macAlgorithm, uint8_t *keyId, const uint8_t *message, size_t messageSize,
        const uint8_t *signature, size_t signatureSize);

#ifdef __cplusplus
} 
#endif

#endif 
