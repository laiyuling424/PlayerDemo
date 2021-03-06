//
// Created by 赖於领 on 2021/9/3.
//

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:05 下午
 */
#ifndef LYLPLAYERDEMO_BASECHANNEL_H
#define LYLPLAYERDEMO_BASECHANNEL_H


extern "C" {
#include <libavcodec/avcodec.h>
};

#include "lock_queue.h"
#include "JavaCallHelper.h"
#include "Constant.h"

class BaseChannel {
public:
    JavaCallHelper *javaCallHelper;
    AVRational time_base;
    double clock = 0;
    AVCodecContext *avCodecContext;
    LockQueue<AVPacket *> package_queue;
    LockQueue<AVFrame *> frame_queue;
    volatile int channelId;
    volatile bool isPlaying;

public:
    BaseChannel(AVCodecContext *context, AVRational time, int id,
                JavaCallHelper *javaCallHelper) : avCodecContext(context), time_base(time),
                                                  channelId(id), javaCallHelper(javaCallHelper) {
        package_queue.setReleaseData(releaseAvPacket);
        frame_queue.setReleaseData(releaseAvFrame);
    }

    virtual ~BaseChannel() {
        if (avCodecContext) {
            avcodec_close(avCodecContext);
            avcodec_free_context(&avCodecContext);
            avCodecContext = 0;
        }
        package_queue.clear();
        frame_queue.clear();
        LOGE("释放channel:%d %d", package_queue.size(), frame_queue.size());
    }

    static void releaseAvPacket(AVPacket *&packet) {
        if (packet) {
            av_packet_free(&packet);
            packet = 0;
        }
    }

    static void releaseAvFrame(AVFrame *&frame) {
        if (frame) {
            av_frame_free(&frame);
            frame = 0;
        }
    }

    virtual void play() = 0;

    virtual void stop() = 0;

    virtual void seek(int time) = 0;

    virtual void speed(int s) = 0;

    virtual void pause() = 0;

    virtual void resume() = 0;

    virtual void release() {
        isPlaying = false;
        javaCallHelper = NULL;

        if (avCodecContext != NULL) {
            avcodec_close(avCodecContext);
            avcodec_free_context(&avCodecContext);
            avCodecContext = NULL;
        }

        package_queue.destory();
        frame_queue.destory();
    };
};

#endif //LYLPLAYERDEMO_BASECHANNEL_H
