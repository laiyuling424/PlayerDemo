//
// Created by 赖於领 on 2021/9/3.
//

#ifndef LYLPLAYERDEMO_FFMPEGPLAYER_H
#define LYLPLAYERDEMO_FFMPEGPLAYER_H

#include <jni.h>
#include <pthread.h>
#include "VideoChannel.h"
#include "Constant.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
}


/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:30 下午
 */
class FFmpegPlayer {
private:
    char *url;
    bool isPlaying;
    JavaVM *javaVM;
    VideoChannel *videoChannel;
    jobject surface;
//    AudioChannel *audioChannel;
    AVFormatContext *formatContext;
    JavaCallHelper *javaCallHelper;
    pthread_t prepare_pid;
    pthread_t play_pid;
public:
    void setPath(char *path);

    void setSurface(jobject surface);

    void prepare();

    void prepareFFmpeg();

    /**
     * 将音频包视频包给相应的队列进行播放
     */
    void start();

    void play();

    void stop();

    void seek(int time);

    void error(int code, char *message);


    FFmpegPlayer(JavaVM *javaVM, jobject jclass);

    ~FFmpegPlayer();
};


#endif //LYLPLAYERDEMO_FFMPEGPLAYER_H

