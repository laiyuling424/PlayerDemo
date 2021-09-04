//
// Created by 赖於领 on 2021/9/3.
//

#ifndef LYLPLAYERDEMO_VIDEOCHANNEL_H
#define LYLPLAYERDEMO_VIDEOCHANNEL_H


/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:28 下午
 */

#include "BaseChannel.h"
#include <jni.h>

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libswresample/swresample.h"
}
typedef void (*RenderFrame)(int, int, uint8_t *, int);

class VideoChannel : public BaseChannel {

private:
    pthread_t play_pid;
    pthread_t decode_pid;
    RenderFrame renderFrame;

public:

    VideoChannel(AVCodecContext *context, AVRational time, int id, JavaCallHelper *javaCallHelper);

    ~VideoChannel();

    virtual void play();

    virtual void stop();

    virtual void seek(int time);

    virtual void speed(int s);

    void decodePacket();

    void render();

    void setFrameRender(RenderFrame renderFrame);
};


#endif //LYLPLAYERDEMO_VIDEOCHANNEL_H

