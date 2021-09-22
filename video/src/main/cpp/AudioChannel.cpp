//
// Created by 赖於领 on 2021/9/7.
//

#include "AudioChannel.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/7 11:08 上午
 */

void *pthread_audio_decode(void *context) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(context);
    audioChannel->decodeAudioPacket();
    return 0;
}

void *pthread_audio_play(void *context) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(context);
    audioChannel->init_opensl_es();
    return 0;
}

int AudioChannel::getPackageSize() {
    int data_size = 0;
    int ret;
    AVFrame *avFrame = 0;
    while (isPlaying) {
        ret = frame_queue.pop(avFrame);
//        LOGE("AudioChannel frame_queue pop size is %d", frame_queue.size());
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }


        uint64_t dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swrContext, avFrame->sample_rate) + avFrame->nb_samples,
                out_sample_rate,
                avFrame->sample_rate,
                AV_ROUND_UP);
        // 转换，返回值为转换后的sample个数  buffer malloc（size）
        int nb = swr_convert(swrContext, &buffer, dst_nb_samples,
                             (const uint8_t **) avFrame->data, avFrame->nb_samples);
        //判断这个是因为之前我直接通过 swr_alloc_set_opts() 拿到了 SwrContext， 之后没有对 SwrContext 进行初始化 swr_init() 在 swr_convert() 报了一个 Invalid argument的错误
        if (nb < 0) {
            char buf[1024];
            av_strerror(ret, buf, 1024);
            LOGE("swr_convert error  %d(%s)", ret, buf);//Invalid argument
            continue;
        }
//            int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
//            data_size = ret * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
//      //转换后多少数据  buffer size  44110*2*2
        data_size = nb * out_channels * out_samplesize;
        clock = avFrame->pts * av_q2d(time_base);
//        LOGE("解码一帧音频  %d,clock is %f,last_time is %f", frame_queue.size(), clock, last_time);
        if (abs(clock - last_time) > 1.0) {
            last_time = clock;
            javaCallHelper->call_java_videoInfo(THREAD_CHILD, 60, static_cast<int>(clock));
        }

        break;
    }
    releaseAvFrame(avFrame);
    return data_size;
}

void audioChannelBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context) {
    //assert(NULL == context);
    AudioChannel *audioChannel = static_cast<AudioChannel *>(context);
    int size = audioChannel->getPackageSize();
    if (size > 0) {
//        LOGE("size is %d", size);
        SLresult result;
        // enqueue another buffer
        result = (*bf)->Enqueue(bf, audioChannel->buffer, size);
    } else {
        LOGE("size null");
    }

}


AudioChannel::AudioChannel(AVCodecContext *context, AVRational time, int id, JavaCallHelper *javaCallHelper)
        : BaseChannel(context, time, id, javaCallHelper) {
    //根据布局获取声道数
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_samplesize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100;
    buffer = (uint8_t *) malloc(out_sample_rate * out_samplesize * out_channels);
}

AudioChannel::~AudioChannel() {

}


void AudioChannel::decodeAudioPacket() {
    int ret;
    AVPacket *packet = 0;
    while (this->isPlaying) {
        ret = package_queue.pop(packet);
//        LOGE("AudioChannel package_queue pop size is %d", package_queue.size());
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        ret = avcodec_send_packet(avCodecContext, packet);
        releaseAvPacket(packet);
        if (ret == AVERROR(EAGAIN)) {
            //需要更多数据
            continue;
        } else if (ret < 0) {
            //失败
            LOGE("AudioChannel avcodec_send_packet errors");
            break;
        }
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, avFrame);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF)) {
            break;
        } else if (ret < 0) {
            LOGE("AudioChannel legitimate decoding 2 errors");
            break;
        }

        while (frame_queue.size() > 100 && isPlaying) {
            av_usleep(1000 * 10);
            continue;
        }
        frame_queue.push(avFrame);
//        LOGE("AudioChannel frame_queue push size is %d", frame_queue.size());
    }
//    releaseAvPacket(packet);
    LOGE("AudioChannel decodeAudioPacket thread end");
}

void AudioChannel::play() {
    swrContext = swr_alloc_set_opts(0,
                                    AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                                    avCodecContext->channel_layout,
                                    avCodecContext->sample_fmt,
                                    avCodecContext->sample_rate, 0, 0);
    swr_init(swrContext);
    if (swr_init(swrContext) < 0) {
        LOGE("swr_init error");
        return;
    }
    this->package_queue.setWork(true);
    this->frame_queue.setWork(true);
    this->isPlaying = true;
    pthread_create(&decode_pid, NULL, pthread_audio_decode, this);
    pthread_create(&play_pid, NULL, pthread_audio_play, this);
    pthread_detach(play_pid);
    pthread_detach(decode_pid);
}

void AudioChannel::pause() {
    isPlaying = false;
//    this->package_queue.setWork(false);
//    this->frame_queue.setWork(false);
    //5.设置播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED);
}

void AudioChannel::resume() {
    /**
    * TODO 疑惑 为什么加上下面两句话在 pause 后 resume 会 anr
    */
//    this->package_queue.setWork(true);
//    this->frame_queue.setWork(true);
    this->isPlaying = true;
    pthread_create(&decode_pid, NULL, pthread_audio_decode, this);
    pthread_detach(decode_pid);
    //5.设置播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
}

void AudioChannel::stop() {
    isPlaying = false;
    package_queue.clear();
    frame_queue.clear();
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
}

void AudioChannel::seek(int time) {
//    avcodec_flush_buffers(avCodecContext);
    //要不要让队列停止工作
    package_queue.clear();
    frame_queue.clear();
    //要不要让队列恢复工作
}

void AudioChannel::speed(int s) {

}


void AudioChannel::init_opensl_es() {

    //执行结果
    SLresult result;
    //引擎对象接口
    SLObjectItf engineObj;
    //引擎对象实例
    SLEngineItf engineItf;

    //1.创建引擎 三步走
    result = slCreateEngine(&engineObj, 0, 0, 0, 0, 0);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engineItf);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //2.创建混音器 这些参数大多都是从 google simple 中拿的，具体的含义不太清楚
    SLObjectItf mixerObj;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
    SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineItf)->CreateOutputMix(engineItf, &mixerObj, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*mixerObj)->Realize(mixerObj, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*mixerObj)->GetInterface(mixerObj, SL_IID_ENVIRONMENTALREVERB,
                                       &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }

    //3.创建播放器
    SLObjectItf playerObj;

    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,//播放pcm格式的数据
            2,//2个声道（立体声）
            SL_SAMPLINGRATE_44_1,//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    SLDataSource pAudioSrc = {&android_queue, &pcm};

    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, mixerObj};
    SLDataSink pAudioSnk = {&loc_outmix, NULL};

    SLuint32 numInterfaces = 3;
    const SLInterfaceID pInterfaceIds[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/};

    const SLboolean pInterfaceRequired[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ };

    result = (*engineItf)->CreateAudioPlayer(engineItf, &playerObj, &pAudioSrc, &pAudioSnk,
                                             numInterfaces,
                                             pInterfaceIds, pInterfaceRequired);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*playerObj)->GetInterface(playerObj, SL_IID_PLAY, &playItf);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //4.设置回调队列及回调函数
    SLVolumeItf pcmPlayerVolume = NULL;

    result = (*playerObj)->GetInterface(playerObj, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, audioChannelBufferCallBack, this);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //获取音量接口
    result = (*playerObj)->GetInterface(playerObj, SL_IID_VOLUME, &pcmPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //5.设置播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

    //6.回调
    audioChannelBufferCallBack(bqPlayerBufferQueue, this);
    LOGE("--- 手动调用播放 packet:%d", this->package_queue.size());
}

void AudioChannel::release() {
    isPlaying = false;
    BaseChannel::release();
    free(buffer);
    buffer = NULL;
    pthread_join(play_pid, NULL);
    pthread_join(decode_pid, NULL);
}








