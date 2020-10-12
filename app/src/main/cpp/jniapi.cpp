#include <jni.h>
#include <stdint.h>
#include <thread>
#include <camera/NdkCameraManager.h>

#include <media/NdkImageReader.h>

#include "log.h"
#include "cam_utils.h"

using namespace cam;

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

static GLuint textureId;

static AImageReader* imageReader = nullptr;

static void imageCallback(void* context, AImageReader* reader)
{
    AImage *image = nullptr;
    auto statuc = AImageReader_acquireNextImage(reader, &image);
    LOGD("imageCallback()");

    std::thread processor([=]() {
        uint8_t *data = nullptr;
        int len = 0;
        AImage_getPlaneData(image, 0, &data, &len);

        // Process data here
        // ...

        AImage_delete(image);
    });
    processor.detach();
}

AImageReader* createJpegReader()
{
    AImageReader* reader = nullptr;

    // TODO: AImageReader_new() expects width and height as the first two arguments. Here you should choose values compactible with the camera device (see previous section for how to query supported resolutions).
    media_status_t status = AImageReader_new(640, 480, AIMAGE_FORMAT_JPEG, 4, &reader);

    if (status != AMEDIA_OK)
        return nullptr;

    AImageReader_ImageListener listener {
        .context = nullptr,
        .onImageAvailable = imageCallback,
    };
    AImageReader_setImageListener(reader, &listener);

    return reader;
}

static void initCam(JNIEnv* env, jobject surface)
{
    //
}

static void initSurface(JNIEnv* env, jint texId, jobject surface) {
    // We can use the id to bind to GL_TEXTURE_EXTERNAL_OES
    textureId = texId;

    // Prepare the surfaces/targets & initialize session
    initCam(env, surface);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_MainActivity_nativeTest(JNIEnv *env, jobject thiz) {
    // TODO: implement nativeTest()
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_CamRenderer_nativeOnSurfaceCreated(JNIEnv *env, jobject thiz,
                                                           jint texture_id, jobject surface) {
    initSurface(env, texture_id, surface);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_CamRenderer_nativeOnDrawFrame(JNIEnv *env, jobject thiz,
                                                      jfloatArray tex_matrix) {
    // TODO: implement nativeOnDrawFrame()
}