#include "logger.h"
#include "primitives.h"

#include <android/native_window_jni.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>

#include <jni.h>
#include <vector>
#include <string>
#include <cassert>
#include <pthread.h>
#include <mutex>

static const char* activeCameraId = nullptr;
static ANativeWindow* nativeWindow = nullptr;
static ACameraManager* cameraManager = nullptr;
static ACameraIdList* cameraIdList = nullptr;
static ACameraDevice* cameraDevice = nullptr;
static ACaptureRequest* captureRequest = nullptr;
static ACameraOutputTarget* cameraOutputTarget = nullptr;
static ACaptureSessionOutput* sessionOutput = nullptr;
static ACaptureSessionOutputContainer* captureSessionOutputContainer = nullptr;
static ACameraCaptureSession* captureSession = nullptr;

static ACameraDevice_StateCallbacks deviceStateCallbacks;
static ACameraCaptureSession_stateCallbacks captureSessionStateCallbacks;

static void onCameraDeviceDisconnected(void *context, ACameraDevice *device) {
    DUAL_INFO("Camera(id: %s) is disconnected.\n", ACameraDevice_getId(device));
}

static void onCameraDeviceError(void *context, ACameraDevice *device, int error) {
    DUAL_INFO("Error(code: %d) on Camera(id: %s).\n", error, ACameraDevice_getId(device));
}

static void onCaptureSessionReady(void *context, ACameraCaptureSession *session) {
    DUAL_INFO("Session is ready. %p\n", session);
}

static void onCaptureSessionActive(void *context, ACameraCaptureSession *session) {
    DUAL_INFO("Session is activated. %p\n", session);
}

static void onCaptureSessionClosed(void *context, ACameraCaptureSession *session) {
    DUAL_INFO("Session is closed. %p\n", session);
}

static camera_status_t openCamera(const char* cameraId) {
    deviceStateCallbacks.onDisconnected = onCameraDeviceDisconnected;
    deviceStateCallbacks.onError = onCameraDeviceError;

    if (cameraDevice) {
        DUAL_ERR("Cannot open camera before closing previously open one");
        return ACAMERA_ERROR_INVALID_PARAMETER;
    }

    activeCameraId = cameraId;
    return ACameraManager_openCamera(
            cameraManager, cameraId,
            &deviceStateCallbacks, &cameraDevice
    );
}

static camera_status_t closeCamera() {
    camera_status_t status = ACameraDevice_close(cameraDevice);
    cameraDevice = nullptr;
    return status;
}

static void openCaptureSession(ACameraDevice_request_template templateId, jobject surface) {
    camera_status_t errorStatus = ACameraDevice_createCaptureRequest(
            cameraDevice,
            templateId,
            &captureRequest
    );

    if (errorStatus != ACAMERA_OK) {
        DUAL_ERR("Failed to create capture request (id: %s)\n", activeCameraId);
    }

    ACaptureSessionOutputContainer_create(&captureSessionOutputContainer);

    captureSessionStateCallbacks.onReady = onCaptureSessionReady;
    captureSessionStateCallbacks.onActive = onCaptureSessionActive;
    captureSessionStateCallbacks.onClosed = onCaptureSessionClosed;

    DUAL_INFO("Surface is prepared in %p.\n", surface);

    ACameraOutputTarget_create(nativeWindow, &cameraOutputTarget);
    ACaptureRequest_addTarget(captureRequest, cameraOutputTarget);

    ACaptureSessionOutput_create(nativeWindow, &sessionOutput);
    ACaptureSessionOutputContainer_add(captureSessionOutputContainer, sessionOutput);

    ACameraDevice_createCaptureSession(
            cameraDevice, captureSessionOutputContainer,
            &captureSessionStateCallbacks, &captureSession
    );

    ACameraCaptureSession_setRepeatingRequest(
            captureSession, nullptr, 1,
            &captureRequest, nullptr
    );
}

static void closeCaptureSession() {
    camera_status_t camera_status = ACAMERA_OK;

    if (captureRequest) {
        ACaptureRequest_free(captureRequest);
        captureRequest = nullptr;
    }

    if (cameraOutputTarget) {
        ACameraOutputTarget_free(cameraOutputTarget);
        cameraOutputTarget = nullptr;
    }

    if (sessionOutput) {
        ACaptureSessionOutput_free(sessionOutput);
        sessionOutput = nullptr;
    }

    if (captureSessionOutputContainer) {
        ACaptureSessionOutputContainer_free(captureSessionOutputContainer);
        captureSessionOutputContainer = nullptr;
    }

    DUAL_INFO("Capture session closed!\n");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_Dual_init(JNIEnv *env, jobject thiz) {
    cameraManager = ACameraManager_create();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_Dual_release(JNIEnv *env, jobject thiz) {
    if (cameraManager) {
        ACameraManager_delete(cameraManager);
        cameraManager = nullptr;
    }
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_desperate_dual_NativeCamera_getIdList(JNIEnv *env, jobject thiz) {
    jobjectArray strArray;

    camera_status_t status = ACameraManager_getCameraIdList(cameraManager, &cameraIdList);

    if (status != ACAMERA_OK || !cameraIdList) {
        DUAL_ERR("Get camera id list failed: status %d, cameraIdList %p", status, cameraIdList);
        ACameraManager_deleteCameraIdList(cameraIdList);
        return (jobjectArray) env->NewObjectArray(
                0,
                env->FindClass("java/lang/String"),
                env->NewStringUTF("")
        );
    }

    DUAL_INFO("Number of cameras: %d", cameraIdList->numCameras);
    strArray = (jobjectArray) env->NewObjectArray(
            cameraIdList->numCameras,
            env->FindClass("java/lang/String"),
            env->NewStringUTF("")
    );

    for (u32 i = 0; i < cameraIdList->numCameras; i++) {
        DUAL_INFO("Camera ID: %s", cameraIdList->cameraIds[i]);
        env->SetObjectArrayElement(strArray, i, env->NewStringUTF(cameraIdList->cameraIds[i]));
    }

    ACameraManager_deleteCameraIdList(cameraIdList);

    return strArray;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_desperate_dual_NativeCamera_getLensId(JNIEnv *env, jobject thiz, jstring id) {
    const char* cameraId = env->GetStringUTFChars(id, nullptr);

    ACameraMetadata* metadata;
    camera_status_t errorStatus;
    ACameraMetadata_const_entry entry;

    errorStatus = ACameraManager_getCameraCharacteristics(cameraManager, cameraId, &metadata);
    if (errorStatus != ACAMERA_OK) {
        DUAL_ERR("Get camera characteristics failed: status=%d", errorStatus);
        if (metadata) {
            ACameraMetadata_free(metadata);
        }
        return -1;
    }

    errorStatus = ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &entry);
    if (errorStatus != ACAMERA_OK) {
        DUAL_ERR("Get camera metadata const entry failed: status=%d", errorStatus);
        ACameraMetadata_free(metadata);
        return -1;
    }

    // validate metadata entry data

    if (entry.type != ACAMERA_TYPE_BYTE) {
        DUAL_ERR("Camera metadata const entry.type is %u but should be %u.",
                 entry.type, ACAMERA_TYPE_BYTE);
        return -1;
    }

    if (entry.count != 1) {
        DUAL_ERR("Camera metadata const entry.count is %u but should be %u.", entry.count, 1);
        return -1;
    }

    if (!entry.data.u8) {
        DUAL_ERR("Camera metadata entry.data.u8 is null");
        return -1;
    }

    return *(entry.data.u8);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_desperate_dual_NativeCamera_getLensName(JNIEnv *env, jobject thiz, jint lens_id) {
    switch (lens_id) {
        case ACAMERA_LENS_FACING_BACK: return env->NewStringUTF("Back Camera");
        case ACAMERA_LENS_FACING_FRONT: return env->NewStringUTF("Front Camera");
        case ACAMERA_LENS_FACING_EXTERNAL: return env->NewStringUTF("External Camera");
        default: return env->NewStringUTF("Unknown Camera");
    }
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_desperate_dual_NativeCamera_open(JNIEnv *env, jobject thiz, jstring camera_id) {
    return openCamera(env->GetStringUTFChars(camera_id, nullptr)) == ACAMERA_OK;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_desperate_dual_NativeCamera_close(JNIEnv *env, jobject thiz) {
    return closeCamera() == ACAMERA_OK;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_NativeCamera_startPreview(JNIEnv *env, jobject thiz, jobject surface) {
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    openCaptureSession(TEMPLATE_PREVIEW, surface);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_NativeCamera_stopPreview(JNIEnv *env, jobject thiz) {
    closeCaptureSession();
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
}