package com.example.camera

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.AttributeSet

class CamSurfaceView : GLSurfaceView {

    var camRenderer : CamRenderer

    constructor(context: Context?) : super(context) {
        setEGLContextClientVersion(2)

        camRenderer = CamRenderer()
        setRenderer(camRenderer)
    }
}