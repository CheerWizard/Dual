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
#include <unordered_map>
#include <sstream>

struct Camera {
    u32 id = 0;
    ANativeWindow* nativeWindow = nullptr;
    ACameraDevice* device = nullptr;
    ACaptureRequest* captureRequest = nullptr;
    ACameraOutputTarget* outputTarget = nullptr;
    ACaptureSessionOutput* sessionOutput = nullptr;
    ACaptureSessionOutputContainer* sessionOutputContainer = nullptr;
    ACameraCaptureSession* captureSession = nullptr;

    Camera() = default;
    Camera(u32 id) : id(id) {}
};

static ACameraManager* cameraManager = nullptr;
static ACameraIdList* cameraIdList = nullptr;

static std::unordered_map<u32, Camera> cameras;

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

template<typename TO>
static TO convert(const char* from) {
    std::stringstream ss;
    ss << from;
    TO to;
    ss >> to;
    return to;
}

static void createCameras(const char** cameraIds, u32 cameraCount) {
    DUAL_INFO("createCameras()");
    for (u32 i = 0; i < cameraCount; i++) {
        auto id = convert<u32>(cameraIds[i]);
        cameras.insert(std::pair<u32, Camera>(id, Camera(id)));
        DUAL_INFO("Added new camera id=%u", id);
    }
}

static void createCallbacks() {
    deviceStateCallbacks.onDisconnected = onCameraDeviceDisconnected;
    deviceStateCallbacks.onError = onCameraDeviceError;
    captureSessionStateCallbacks.onReady = onCaptureSessionReady;
    captureSessionStateCallbacks.onActive = onCaptureSessionActive;
    captureSessionStateCallbacks.onClosed = onCaptureSessionClosed;
}

static camera_status_t openCamera(const char* cameraId) {
    Camera& camera = cameras.at(convert<u32>(cameraId));
    auto status = ACameraManager_openCamera(
            cameraManager, cameraId,
            &deviceStateCallbacks, &camera.device
    );
    DUAL_INFO("openCamera(): cameraId=%s camera.device=%p status=%u", cameraId, camera.device, status);
    return status;
}

static camera_status_t closeCamera(u32 cameraId) {
    Camera& camera = cameras.at(cameraId);
    camera_status_t status = ACameraDevice_close(camera.device);
    camera.device = nullptr;
    return status;
}

static camera_status_t closeCamera(const char* cameraId) {
    return closeCamera(convert<u32>(cameraId));
}

static void releaseCameras() {
    for (auto camera : cameras) {
        closeCamera(camera.first);
    }
    cameras.clear();
}

static void openCaptureSession(ACameraDevice_request_template templateId, jobject surface, Camera& camera) {
    DUAL_INFO("openCaptureSession(): camera.device=%p", camera.device);

    camera_status_t errorStatus = ACameraDevice_createCaptureRequest(
            camera.device,
            templateId,
            &camera.captureRequest
    );

    if (errorStatus != ACAMERA_OK) {
        DUAL_ERR("Failed to create capture request (id: %u)\n", camera.id);
    }

    ACaptureSessionOutputContainer_create(&camera.sessionOutputContainer);

    DUAL_INFO("Surface is prepared in %p.\n", surface);

    ACameraOutputTarget_create(camera.nativeWindow, &camera.outputTarget);
    ACaptureRequest_addTarget(camera.captureRequest, camera.outputTarget);

    ACaptureSessionOutput_create(camera.nativeWindow, &camera.sessionOutput);
    ACaptureSessionOutputContainer_add(camera.sessionOutputContainer, camera.sessionOutput);

    ACameraDevice_createCaptureSession(
            camera.device, camera.sessionOutputContainer,
            &captureSessionStateCallbacks, &camera.captureSession
    );

    ACameraCaptureSession_setRepeatingRequest(
            camera.captureSession, nullptr, 1,
            &camera.captureRequest, nullptr
    );
}

static void closeCaptureSession(Camera& camera) {
    if (camera.captureRequest) {
        ACaptureRequest_free(camera.captureRequest);
        camera.captureRequest = nullptr;
    }

    if (camera.outputTarget) {
        ACameraOutputTarget_free(camera.outputTarget);
        camera.outputTarget = nullptr;
    }

    if (camera.sessionOutput) {
        ACaptureSessionOutput_free(camera.sessionOutput);
        camera.sessionOutput = nullptr;
    }

    if (camera.sessionOutputContainer) {
        ACaptureSessionOutputContainer_free(camera.sessionOutputContainer);
        camera.sessionOutputContainer = nullptr;
    }

    DUAL_INFO("Capture session closed!\n");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_Dual_init(JNIEnv *env, jobject thiz) {
    cameraManager = ACameraManager_create();
    createCallbacks();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_Dual_release(JNIEnv *env, jobject thiz) {
    releaseCameras();
    if (cameraManager) {
        ACameraManager_delete(cameraManager);
        cameraManager = nullptr;
    }
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_desperate_dual_NativeCamera_getIdList(JNIEnv *env, jobject thiz) {
    jobjectArray ids;

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
    ids = (jobjectArray) env->NewObjectArray(
            cameraIdList->numCameras,
            env->FindClass("java/lang/String"),
            env->NewStringUTF("")
    );

    for (u32 i = 0; i < cameraIdList->numCameras; i++) {
        DUAL_INFO("Camera ID: %s", cameraIdList->cameraIds[i]);
        env->SetObjectArrayElement(ids, i, env->NewStringUTF(cameraIdList->cameraIds[i]));
    }

    releaseCameras();
    createCameras(cameraIdList->cameraIds, cameraIdList->numCameras);
    ACameraManager_deleteCameraIdList(cameraIdList);

    return ids;
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
Java_com_desperate_dual_NativeCamera_open(JNIEnv *env, jobject thiz, jstring id) {
    return openCamera(env->GetStringUTFChars(id, nullptr)) == ACAMERA_OK;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_desperate_dual_NativeCamera_close(JNIEnv *env, jobject thiz, jstring id) {
    return closeCamera(env->GetStringUTFChars(id, nullptr)) == ACAMERA_OK;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_NativeCamera_startPreview(JNIEnv *env, jobject thiz, jobject surface, jstring id) {
    Camera& camera = cameras.at(convert<u32>(env->GetStringUTFChars(id, nullptr)));
    camera.nativeWindow = ANativeWindow_fromSurface(env, surface);
    openCaptureSession(TEMPLATE_PREVIEW, surface, camera);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_desperate_dual_NativeCamera_stopPreview(JNIEnv *env, jobject thiz, jstring id) {
    Camera& camera = cameras.at(convert<u32>(env->GetStringUTFChars(id, nullptr)));

    closeCaptureSession(camera);
    if (camera.nativeWindow) {
        ANativeWindow_release(camera.nativeWindow);
        camera.nativeWindow = nullptr;
    }
}