package com.example.camera

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val view = findViewById<View>(R.id.view)
        view.setOnClickListener(View.OnClickListener {
            nativeTest()
        })
    }

    override fun onResume() {
        super.onResume()

        // ask for camera permission if necessary
        val camPermission = Manifest.permission.CAMERA
        if (Build.VERSION.SDK_INT >= 23 &&
                checkSelfPermission(camPermission) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(camPermission), CAM_PERMISSION_CODE)
        } else {
            // initCam()
            // camSurfaceView.onResume()
        }

        nativeTest()
    }

    external fun nativeTest()

    companion object {
        val CAM_PERMISSION_CODE = 666

        init {
            System.loadLibrary("jniapi")
        }
    }
}