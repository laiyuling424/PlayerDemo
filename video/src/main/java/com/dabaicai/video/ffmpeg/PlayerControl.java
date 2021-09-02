package com.dabaicai.video.ffmpeg;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/2 7:30 下午
 */
public class PlayerControl implements SurfaceHolder.Callback {


    public enum PlayerStatus {
        NONE(0),
        PREPARE(1),
        PLAYING(2),
        STOP(3),
        DESTORY(4),
        ACTIONDO(5);


        int status;

        private PlayerStatus(int code) {
            this.status = code;
        }
    }

    enum Speed {
        normal,
        time2,
        time3
    }

    static {
        System.loadLibrary("native-lib");
    }

    private PlayerStatus playerStatus;
    private String path;
    private int videoTime;
    private SurfaceHolder surfaceHolder;
    private PlayerControlCallBack playerControlCallBack;

    public PlayerControl() {
        playerStatus = PlayerStatus.NONE;
    }


    private void startOrStop() {
        if (PlayerStatus.NONE == playerStatus) {
            native_prepare(path);
        } else if (PlayerStatus.PLAYING == playerStatus) {
            native_stop();
        } else if (PlayerStatus.STOP == playerStatus) {
            native_start();
        }
    }

    private void setCallBack(PlayerControlCallBack playerControlCallBack) {
        this.playerControlCallBack = playerControlCallBack;
    }

    private void setSurfaceView(SurfaceView surfaceView) {
        if (null != this.surfaceHolder) {
            this.surfaceHolder.removeCallback(this);
        }
        this.surfaceHolder = surfaceView.getHolder();
        this.surfaceHolder.addCallback(this);
    }


    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        native_set_surface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    private native void native_prepare(String path);

    private native void native_set_surface(Surface surface);

    private native void native_start();

    private native void native_stop();

    private native void native_seek();

    private native void native_speed(int speed);

    private native void native_change_path(String path);

    private void error(int code, String message) {
        if (null != playerControlCallBack) {
            playerControlCallBack.error(code, message);
        }
    }

    private void ready(int alltime) {
        videoTime = alltime;
        if (null != playerControlCallBack) {
            playerControlCallBack.ready(alltime);
        }
    }

    private void status(int status) {
        if (status == 0) {
            playerStatus = PlayerStatus.NONE;
        } else if (status == 1) {
            playerStatus = PlayerStatus.PREPARE;
            native_start();
        } else if (status == 2) {
            playerStatus = PlayerStatus.PLAYING;
        } else if (status == 3) {
            playerStatus = PlayerStatus.STOP;
        } else if (status == 4) {
            playerStatus = PlayerStatus.DESTORY;
        } else if (status == 5) {
            playerStatus = PlayerStatus.ACTIONDO;
        }
        if (null != playerControlCallBack) {
            playerControlCallBack.status(playerStatus);
        }
    }

    private void videoInfo(int fps, int time) {
        if (null != playerControlCallBack) {
            playerControlCallBack.videoInfo(fps, time, videoTime);
        }
    }

    public interface PlayerControlCallBack {

        /**
         * c++ 错误时的回调接口
         *
         * @param code    错误 code
         * @param message 错误信息
         */
        void error(int code, String message);

        /**
         * C++ 层初始化完毕 可以开始播放
         *
         * @param alltime 视频的时长
         */
        void ready(int alltime);

        /**
         * C++ 层的状态
         *
         * @param status
         */
        default void status(int status) {
        }

        void status(PlayerStatus status);

        /**
         * @param fps  视频的帧率
         * @param time 视频的进度
         */
        void videoInfo(int fps, int time, int allTime);

    }

}