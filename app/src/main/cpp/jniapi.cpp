#include <jni.h>
#include <stdint.h>
#include <thread>
#include <vector>
#include <camera/NdkCameraManager.h>
#include <media/NdkImageReader.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>
#include <android/native_window_jni.h>

#include "log.h"
#include "cam_utils.h"

using namespace cam;

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

static GLuint textureId;

static ACameraManager* cameraManager = nullptr;
static ACameraDevice* cameraDevice = nullptr;

static AImageReader* imageReader = nullptr;

static ANativeWindow* textureWindow = nullptr;
static ACameraOutputTarget* textureTarget = nullptr;
static ACaptureSessionOutput* textureOutput = nullptr;
static ACameraCaptureSession* textureSession = nullptr;

static ANativeWindow* imageWindow = nullptr;
static ACameraOutputTarget* imageTarget = nullptr;
static ACaptureSessionOutput* imageOutput = nullptr;

static ACaptureRequest* request = nullptr;

static ACaptureSessionOutput* output = nullptr;
static ACaptureSessionOutputContainer* outputs = nullptr;

/**
 * GL stuff - mostly used to draw the frames captured
 * by camera into a SurfaceTexture
 */

static GLuint prog;
static GLuint vtxShader;
static GLuint fragShader;

static GLint vtxPosAttrib;
static GLint uvsAttrib;
static GLint mvpMatrix;
static GLint texMatrix;
static GLint texSampler;
static GLint color;
static GLint size;
static GLuint buf[2];

static int width = 640;
static int height = 480;

static const char* vertexShaderSrc = R"(
        precision highp float;
        attribute vec3 vertexPosition;
        attribute vec2 uvs;
        varying vec2 varUvs;
        uniform mat4 texMatrix;
        uniform mat4 mvp;

        void main()
        {
            varUvs = (texMatrix * vec4(uvs.x, uvs.y, 0, 1.0)).xy;
            gl_Position = mvp * vec4(vertexPosition, 1.0);
        }
)";

static const char* fragmentShaderSrc = R"(
        #extension GL_OES_EGL_image_external : require
        precision mediump float;

        varying vec2 varUvs;
        uniform samplerExternalOES texSampler;
        uniform vec4 color;
        uniform vec2 size;

        vec2 RGBToCC(vec4 rgba) {
            float Y = 0.299 * rgba.r + 0.587 * rgba.g + 0.114 * rgba.b;
            return vec2((rgba.b - Y) * 0.565, (rgba.r - Y) * 0.713);
        }

        vec4 chromakey(vec4 color, vec4 key)
        {
            if (length(color - key) < 0.8)
                color.b = 1.0;
            return color;
        }

        void main()
        {
            vec4 refColor = vec4(0.05, 0.63, 0.14, 1.0);
            vec4 texColor = texture2D(texSampler, varUvs);
            gl_FragColor = chromakey(texColor, refColor) * color;
        }
)";

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


/**
 * Device listeners
 */

static void onDisconnected(void* context, ACameraDevice* device)
{
    LOGD("onDisconnected");
}

static void onError(void* context, ACameraDevice* device, int error)
{
    LOGD("error %d", error);
}

static ACameraDevice_stateCallbacks cameraDeviceCallbacks = {
        .context = nullptr,
        .onDisconnected = onDisconnected,
        .onError = onError,
};

/**
 * Session state callbacks
 */

static void onSessionActive(void* context, ACameraCaptureSession *session)
{
    LOGD("onSessionActive()");
}

static void onSessionReady(void* context, ACameraCaptureSession *session)
{
    LOGD("onSessionReady()");
}

static void onSessionClosed(void* context, ACameraCaptureSession *session)
{
    LOGD("onSessionClosed()");
}

static ACameraCaptureSession_stateCallbacks sessionStateCallbacks {
        .context = nullptr,
        .onActive = onSessionActive,
        .onReady = onSessionReady,
        .onClosed = onSessionClosed
};

static void initializeCameraDevice()
{
    cameraManager = ACameraManager_create();
    auto id = getBackFacingCamId(cameraManager);
    ACameraManager_openCamera(cameraManager, id.c_str(), &cameraDeviceCallbacks, &cameraDevice);

    printCamProps(cameraManager, id.c_str());
}

static void exitCameraDevice()
{
    if (cameraManager)
    {
        // Stop recording to SurfaceTexture and do some cleanup
        ACameraCaptureSession_stopRepeating(textureSession);
        ACameraCaptureSession_close(textureSession);
        ACaptureSessionOutputContainer_free(outputs);
        ACaptureSessionOutput_free(output);

        ACameraDevice_close(cameraDevice);
        ACameraManager_delete(cameraManager);
        cameraManager = nullptr;

        AImageReader_delete(imageReader);
        imageReader = nullptr;

        // Capture request for SurfaceTexture
        // SIGSEGV
        if (textureWindow)
            ANativeWindow_release(textureWindow);
        textureWindow = nullptr;
        ACaptureRequest_free(request);
    }
}

/**
 * Capture callbacks
 */

void onCaptureFailed(void* context, ACameraCaptureSession* session,
                     ACaptureRequest* request, ACameraCaptureFailure* failure)
{
    LOGE("onCaptureFailed ");
}

void onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session,
                                int sequenceId, int64_t frameNumber)
{}

void onCaptureSequenceAborted(void* context, ACameraCaptureSession* session,
                              int sequenceId)
{}

void onCaptureCompleted (
        void* context, ACameraCaptureSession* session,
        ACaptureRequest* request, const ACameraMetadata* result)
{
    LOGD("Capture completed");
}

static ACameraCaptureSession_captureCallbacks captureCallbacks {
        .context = nullptr,
        .onCaptureStarted = nullptr,
        .onCaptureProgressed = nullptr,
        .onCaptureCompleted = onCaptureCompleted,
        .onCaptureFailed = onCaptureFailed,
        .onCaptureSequenceCompleted = onCaptureSequenceCompleted,
        .onCaptureSequenceAborted = onCaptureSequenceAborted,
        .onCaptureBufferLost = nullptr,
};


ANativeWindow* createSurface(AImageReader* reader)
{
    ANativeWindow *nativeWindow;
    AImageReader_getWindow(reader, &nativeWindow);

    return nativeWindow;
}

static void initCam(JNIEnv* env, jobject surface)
{
    imageReader = createJpegReader();
    imageWindow = createSurface(imageReader);

    // Prepare surface
    textureWindow = ANativeWindow_fromSurface(env, surface);

    // Prepare request for texture target
    ACameraDevice_createCaptureRequest(cameraDevice, TEMPLATE_PREVIEW, &request);

    // Prepare outputs for session
    ACaptureSessionOutputContainer_create(&outputs);

    ACaptureSessionOutput_create(textureWindow, &textureOutput);
    ACaptureSessionOutputContainer_add(outputs, textureOutput);

    // Prepare target surface
    ANativeWindow_acquire(textureWindow);
    ACameraOutputTarget_create(textureWindow, &textureTarget);
    ACaptureRequest_addTarget(request, textureTarget);


    ACaptureSessionOutput_create(imageWindow, &imageOutput);
    ACaptureSessionOutputContainer_add(outputs, imageOutput);

    // Prepare target image surface
    ANativeWindow_acquire(imageWindow);
    ACameraOutputTarget_create(imageWindow, &imageTarget);
    ACaptureRequest_addTarget(request, imageTarget);

    // Create the session
    ACameraDevice_createCaptureSession(cameraDevice, outputs, &sessionStateCallbacks, &textureSession);

    // Start capturing continuously
    ACameraCaptureSession_setRepeatingRequest(textureSession, &captureCallbacks, 1, &request, nullptr);
}

GLuint createShader(const char *src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> logStr(maxLength);

        glGetShaderInfoLog(shader, maxLength, &maxLength, logStr.data());
        LOGE("Could not compile shader %s - %s", src, logStr.data());
    }

    return shader;
}

GLuint createProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);

    GLint isLinked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
        LOGE("Could not link program");

    return prog;
}

void ortho(float *mat4, float left, float right, float bottom, float top, float near, float far)
{
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;

    mat4[0] = 2.0f / rl;
    mat4[12] = -(right + left) / rl;

    mat4[5] = 2.0f / tb;
    mat4[13] = -(top + bottom) / tb;

    mat4[10] = -2.0f / fn;
    mat4[14] = -(far + near) / fn;
}

static void initSurface(JNIEnv* env, jint texId, jobject surface) {
    // We can use the id to bind to GL_TEXTURE_EXTERNAL_OES
    textureId = texId;

    // Prepare the surfaces/targets & initialize session
    initCam(env, surface);

    // Init shaders
    vtxShader = createShader(vertexShaderSrc, GL_VERTEX_SHADER);
    fragShader = createShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);
    prog = createProgram(vtxShader, fragShader);

    // Store attribute and uniform locations
    vtxPosAttrib = glGetAttribLocation(prog, "vertexPosition");
    uvsAttrib = glGetAttribLocation(prog, "uvs");
    mvpMatrix = glGetUniformLocation(prog, "mvp");
    texMatrix = glGetUniformLocation(prog, "texMatrix");
    texSampler = glGetUniformLocation(prog, "texSampler");
    color = glGetUniformLocation(prog, "color");
    size = glGetUniformLocation(prog, "size");

    // Prepare buffers
    glGenBuffers(2, buf);

    // Set up vertices
    float vertices[] {
            // x, y, z, u, v
            -1, -1, 0, 0, 0,
            -1, 1, 0, 0, 1,
            1, 1, 0, 1, 1,
            1, -1, 0, 1, 0
    };
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // Set up indices
    GLuint indices[] { 2, 1, 0, 0, 3, 2 };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
}

static void drawFrame(JNIEnv* env, jfloatArray texMatArray)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glClearColor(0, 0, 0, 1);

    glUseProgram(prog);

    // Configure main transformations
    float mvp[] = {
            1.0f, 0, 0, 0,
            0, 1.0f, 0, 0,
            0, 0, 1.0f, 0,
            0, 0, 0, 1.0f
    };

    float aspect = width > height ? float(width)/float(height) : float(height)/float(width);
    if (width < height) // portrait
        ortho(mvp, -1.0f, 1.0f, -aspect, aspect, -1.0f, 1.0f);
    else // landscape
        ortho(mvp, -aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

    glUniformMatrix4fv(mvpMatrix, 1, false, mvp);


    // Prepare texture for drawing

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Pass SurfaceTexture transformations to shader
    float* tm = env->GetFloatArrayElements(texMatArray, 0);
    glUniformMatrix4fv(texMatrix, 1, false, tm);
    env->ReleaseFloatArrayElements(texMatArray, tm, 0);

    // Set the SurfaceTexture sampler
    glUniform1i(texSampler, 0);

    // I use red color to mix with camera frames
    float c[] = { 1, 1, 1, 1 };
    glUniform4fv(color, 1, (GLfloat*)c);

    // Size of the window is used in fragment shader
    // to split the window
    float sz[2] = {0};
    sz[0] = width;
    sz[1] = height;
    glUniform2fv(size, 1, (GLfloat*)sz);

    // Set up vertices/indices and draw
    glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);

    glEnableVertexAttribArray(vtxPosAttrib);
    glVertexAttribPointer(vtxPosAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0);

    glEnableVertexAttribArray(uvsAttrib);
    glVertexAttribPointer(uvsAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void *)(3 * sizeof(float)));

    glViewport(0, 0, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
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
    drawFrame(env, tex_matrix);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_MainActivity_nativeInitCameraDevice(JNIEnv *env, jobject thiz) {
    initializeCameraDevice();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_MainActivity_nativeExitCameraDevice(JNIEnv *env, jobject thiz) {
    exitCameraDevice();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_camera_CamRenderer_nativeOnSurfaceChanged(JNIEnv *env, jobject thiz, jint w,
                                                           jint h) {
    width = w;
    height = h;
}