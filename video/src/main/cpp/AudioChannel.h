//
// Created by 赖於领 on 2021/9/7.
//

#ifndef LYLPLAYERDEMO_AUDIOCHANNEL_H
#define LYLPLAYERDEMO_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <assert.h>

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
 * date   : 2021/9/7 11:08 上午
 */
class AudioChannel : public BaseChannel {
private:
    pthread_t play_pid;
    pthread_t decode_pid;

    SwrContext *swrContext = NULL;
    int out_channels;
    int out_samplesize;
    int out_sample_rate;

    SLPlayItf playItf;
public:
    //缓冲器队列接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    //缓冲 buffer
    uint8_t *buffer;
//    uint8_t *out_buffer;

public:

    AudioChannel(AVCodecContext *context, AVRational time, int id, JavaCallHelper *javaCallHelper);

    ~AudioChannel();

    virtual void play();

    virtual void stop();

    virtual void seek(int time);

    virtual void speed(int s);

    virtual void pause();

    virtual void resume();

    void decodeAudioPacket();

    void init_opensl_es();

    int getPackageSize();
};


#endif //LYLPLAYERDEMO_AUDIOCHANNEL_H

