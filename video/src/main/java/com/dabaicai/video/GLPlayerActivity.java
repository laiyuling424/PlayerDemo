package com.dabaicai.video;

import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.appcompat.app.AppCompatActivity;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/24 11:55 上午
 */
public class GLPlayerActivity extends AppCompatActivity implements SurfaceHolder.Callback {

    static {
        System.loadLibrary("native-lib");
    }

    SurfaceHolder holder;
    private SurfaceView glSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_glplayer);

        glSurfaceView = findViewById(R.id.glSurfaceView);
        findViewById(R.id.statusButton).setOnClickListener(v -> {
            new Thread() {
                @Override
                public void run() {
                    startplay("/storage/emulated/0/aaa.mp4", holder.getSurface());
                }
            }.start();
        });
        //            File file = new File("/storage/emulated/0/aa.mp4");
        if (null != glSurfaceView.getHolder()) {
            glSurfaceView.getHolder().removeCallback(this);
        }
        glSurfaceView.getHolder().addCallback(this);
    }

    public native void startplay(String path, Surface surface);

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        this.holder = holder;

    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }
}
