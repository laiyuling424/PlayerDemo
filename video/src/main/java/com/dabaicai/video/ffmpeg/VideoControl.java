package com.dabaicai.video.ffmpeg;

import android.view.Surface;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/8/2 9:16 上午
 */
public class VideoControl {

    static {
        System.loadLibrary("native-lib");
    }

    public static void init(Surface surface, String path) {
        native_init(surface, path);
    }

    public static void audioInit(String path) {
        native_audio_init(path);
    }


    //ffmpeg version
    public native static String stringFromJNI();

    //视频播放
    public native static void native_init(Surface surface, String path);

    //generate pcm file
    public native static void native_audio_init(String path);

    //PCM file paly
    public native static void native_opensl_start();

    //audio paly
    public native static void native_audio_play(String path);
}
