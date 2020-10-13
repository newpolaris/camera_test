package com.example.camera

import android.graphics.SurfaceTexture
import android.opengl.GLES11Ext
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.view.Surface
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class CamRenderer: GLSurfaceView.Renderer {

    lateinit var surfaceTexture: SurfaceTexture
    var texMatrix = FloatArray(16)
    @Volatile var frameAvailable: Boolean = false
    var lock = Object()

    override fun onDrawFrame(p0: GL10?) {
        synchronized(lock) {
            if (frameAvailable) {
                surfaceTexture.updateTexImage()
                surfaceTexture.getTransformMatrix(texMatrix)
                frameAvailable = false;
            }
        }

        nativeOnDrawFrame(texMatrix)
    }

    override fun onSurfaceChanged(p0: GL10?, p1: Int, p2: Int) {
        nativeOnSurfaceChanged(p1, p2);
    }

    override fun onSurfaceCreated(p0: GL10?, p1: EGLConfig?) {
        // Prepare texture and surface
        val textures = IntArray(1)
        GLES20.glGenTextures(1, textures, 0)
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textures[0])

        surfaceTexture = SurfaceTexture(textures[0])
        surfaceTexture.setOnFrameAvailableListener {
            synchronized(lock) {
                frameAvailable = true
            }
        }

        val surface = Surface(surfaceTexture)

        // Pass to native code
        nativeOnSurfaceCreated(textures[0], surface)
    }

    external fun nativeOnSurfaceCreated(textureId: Int, surface: Surface)
    external fun nativeOnDrawFrame(texMatrix: FloatArray)
    external fun nativeOnSurfaceChanged(width: Int, height: Int);
}