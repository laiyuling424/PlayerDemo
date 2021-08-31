package com.dabaicai.video;

import android.os.Bundle;
import android.os.Environment;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.dabaicai.video.ffmpeg.VideoControl;

import java.io.File;

public class MainActivity2 extends AppCompatActivity implements SurfaceHolder.Callback {


    TextView textView;
    Button startVideoButton;
    Button startAudioButton;
    Button versionButton;
    Button openslButton;
    Button startPlayAudioButton;
    SurfaceView surfaceView;
    SurfaceHolder surfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main2);

        versionButton = findViewById(R.id.versionButton);
        startVideoButton = findViewById(R.id.startVideoButton);
        startAudioButton = findViewById(R.id.startAudioButton);
        startPlayAudioButton = findViewById(R.id.startPlayAudioButton);
        openslButton = findViewById(R.id.openslButton);
        surfaceView = findViewById(R.id.surfaceView);
        textView = findViewById(R.id.textView);

        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);

        versionButton.setOnClickListener(view ->
                textView.setText(VideoControl.stringFromJNI())
        );

        startVideoButton.setOnClickListener(v -> {
//            File file = new File("/storage/emulated/0/aa.mp4");
            File file = new File(Environment.getExternalStorageDirectory(), "aa.mp4");
            if (file.exists()) {
                VideoControl.init(surfaceHolder.getSurface(), file.getAbsolutePath());
//                VideoControl.native_start(file.getAbsolutePath(), surfaceHolder.getSurface());
            }
//            Log.d("lyll",file.getAbsolutePath());

        });

        startAudioButton.setOnClickListener(v -> {
            File file = new File(Environment.getExternalStorageDirectory(), "aa.mp4");
            if (file.exists()) {
                VideoControl.native_audio_init(file.getAbsolutePath());
            }

        });

        openslButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                VideoControl.native_opensl_start();
            }
        });

        startPlayAudioButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File file = new File(Environment.getExternalStorageDirectory(), "aa.mp4");
                if (file.exists()) {
                    VideoControl.native_audio_play(file.getAbsolutePath());
                }

            }
        });
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {

    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {
        this.surfaceHolder = surfaceHolder;
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {

    }
}