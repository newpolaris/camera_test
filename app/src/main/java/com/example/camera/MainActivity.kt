package com.example.camera

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraMetadata
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.view.SurfaceView
import android.view.View
import android.widget.Toast
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : Activity() {

    lateinit var view: GLSurfaceView;

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)
        view = CamSurfaceView(getApplicationContext())
        setContentView(view)

        if (!isNativeCamSupported()) {
            Toast.makeText(this, "Native camera probably won't work on this device!",
                    Toast.LENGTH_LONG).show()
            finish()
        }
    }

    override fun onResume() {
        super.onResume()

        // ask for camera permission if necessary
        val camPermission = Manifest.permission.CAMERA
        if (Build.VERSION.SDK_INT >= 23 &&
                checkSelfPermission(camPermission) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(camPermission), CAM_PERMISSION_CODE)
        } else {
            nativeInitCameraDevice()
            view.onResume()
        }
    }

    override fun onPause() {
        super.onPause()
        view.onPause()
        nativeExitCameraDevice()
    }

    fun isNativeCamSupported(): Boolean {
        val camManager = getSystemService(Context.CAMERA_SERVICE) as CameraManager
        var nativeCamSupported = false

        for (camId in camManager.cameraIdList) {
            val characteristics = camManager.getCameraCharacteristics(camId)
            val hwLevel = characteristics.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL)
            if (hwLevel != CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
                nativeCamSupported = true
                break
            }
        }

        return nativeCamSupported
    }

    external fun nativeInitCameraDevice()
    external fun nativeExitCameraDevice()

    companion object {
        val CAM_PERMISSION_CODE = 666

        init {
            System.loadLibrary("jniapi")
        }
    }
}