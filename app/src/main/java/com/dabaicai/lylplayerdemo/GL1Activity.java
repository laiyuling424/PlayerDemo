package com.dabaicai.lylplayerdemo;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

import androidx.appcompat.app.AppCompatActivity;

import com.dabaicai.lylplayerdemo.gl.AirHockKeyRender3;


public class GL1Activity extends AppCompatActivity {

    private GLSurfaceView glSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gl1);

        initView();
        initData();
    }

    private void initData() {
        if (supportsEs2()) {
//            AirHockKeyRender myGlRender = new AirHockKeyRender(this);
//            AirHockKeyRender2 myGlRender = new AirHockKeyRender2(this);
//            AirHockKeyRender1 myGlRender = new AirHockKeyRender1(this);
            AirHockKeyRender3 myGlRender = new AirHockKeyRender3(this);
            //设置opengl版本
            if (glSurfaceView == null) {
                Log.d("lyll", "11111");
                return;
            }
            glSurfaceView.setEGLContextClientVersion(2);
            glSurfaceView.setRenderer(myGlRender);
            //RenderMode 有两种，RENDERMODE_WHEN_DIRTY 和 RENDERMODE_CONTINUOUSLY，前者是懒惰渲染，需要手动调用
            // glSurfaceView.requestRender() 才会进行更新，而后者则是不停渲染。
            glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        } else {
            Log.d("mmm", "不支持2.0版本");
        }
    }

    private void initView() {
        glSurfaceView = findViewById(R.id.glsurface);
    }

    private boolean supportsEs2() {
        // Check if the system supports OpenGL ES 2.0.
        ActivityManager activityManager =
                (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo configurationInfo = activityManager
                .getDeviceConfigurationInfo();
        // Even though the latest emulator supports OpenGL ES 2.0,
        // it has a bug where it doesn't set the reqGlEsVersion so
        // the above check doesn't work. The below will detect if the
        // app is running on an emulator, and assume that it supports
        // OpenGL ES 2.0.
        final boolean supportsEs2 =
                configurationInfo.reqGlEsVersion >= 0x20000
                        || (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1
                        && (Build.FINGERPRINT.startsWith("generic")
                        || Build.FINGERPRINT.startsWith("unknown")
                        || Build.MODEL.contains("google_sdk")
                        || Build.MODEL.contains("Emulator")
                        || Build.MODEL.contains("Android SDK built for x86")));

        return supportsEs2;
    }

    @Override
    protected void onResume() {
        super.onResume();
        glSurfaceView.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        glSurfaceView.onPause();
    }
}
