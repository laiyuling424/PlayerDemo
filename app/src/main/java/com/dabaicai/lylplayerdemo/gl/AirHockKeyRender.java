package com.dabaicai.lylplayerdemo.gl;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/13 3:44 下午
 */
public class AirHockKeyRender implements GLSurfaceView.Renderer {
    public AirHockKeyRender(Context context) {

    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        //当surface被创建时，GlsurfaceView会调用这个方法，这个发生在应用程序
        // 第一次运行的时候或者从其他Activity回来的时候也会调用

        GLES20.glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        //在Surface创建以后，每次surface尺寸大小发生变化，这个方法会被调用到，比如横竖屏切换

        GLES20.glViewport(0, 0, width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        //当绘制每一帧数据的时候，会调用这个放方法，这个方法一定要绘制一些东西，即使只是清空屏幕
        //因为这个方法返回后，渲染区的数据会被交换并显示在屏幕上，如果什么都没有话，会看到闪烁效果

        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    }
}

