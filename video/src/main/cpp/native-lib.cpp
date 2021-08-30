#include <jni.h>
#include <string>
#include <android/native_window_jni.h> // window 头文件 渲染画面数据
#include <zconf.h>
#include <android/log.h>  //log 打印类头文件

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <assert.h>

//ffmpeg 是 C 写的，所以要以 C 的方式引入
extern "C" {
#include "libavcodec/avcodec.h"
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#define TAG "lyll"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__) // 定义 log 函数


extern "C"
JNIEXPORT jstring JNICALL
Java_com_dabaicai_video_ffmpeg_VideoControl_stringFromJNI(JNIEnv *env, jclass clazz) {
    return env->NewStringUTF(av_version_info());
}

//==============================================================================
//    int got_picture = 0;
//    AVPacket avPacket;
//    AVFrame *avFrame = av_frame_alloc();
//    AVFrame *rgba_frame = av_frame_alloc();
//    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, avcodec_context->width,
//                                            avcodec_context->height, 1);
//    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
//    while (av_read_frame(avFormatContext, &avPacket) >= 0) {
//        //解码AVPacket->AVFrame
//        avcodec_decode_video2(avcodec_context, avFrame, &got_picture, &avPacket);
//
//        if (got_picture) {
//            ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
//
//            sws_scale(sws_ctx, avFrame->data, avFrame->linesize, 0, avFrame->height,
//                      rgba_frame->data, rgba_frame->linesize);
//
//            memcpy(outBuffer.bits, buffer, avcodec_context->width * avcodec_context->height * 4);
//
//            //unlock
//            ANativeWindow_unlockAndPost(nativeWindow);
//        }
//
//        av_packet_unref(&avPacket);
//    }
//==============================================================================

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_VideoControl_native_1init(JNIEnv *env, jclass clazz,
                                                         jobject surface, jstring path) {

    //先将 path 保存下来，以防函数执行完 path 为null
    const char *_path = env->GetStringUTFChars(path, 0);

    // 1. 注册组件，就是一些初始化工作
    av_register_all();
    avformat_network_init();

    AVFormatContext *avFormatContext = avformat_alloc_context();
    //初始化 ffmpeg 参数
    AVDictionary *options = NULL;

    int ret = -1;

    if (av_dict_set(&options, "timeout", "3000000", 0) < 0) {
        LOGE("av_dict_set error");
        return;
    }

    //NULL 输入文件的封装格式,FFmpeg自动检测 AVInputFormat
    // 2. 打开视频文件
    if ((ret = avformat_open_input(&avFormatContext, _path, NULL, &options)) != 0) {
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("avformat_open_input error Couldn’t open file %s: %d(%s)", _path, ret, buf);
        return;
    }

    // 3. 获取视频信息
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("avformat_find_stream_info error");
        return;
    }

    // 4. 查找视频流
    int video_index = -1;
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            video_index = i;
        }
    }

    if (video_index == -1) {
        LOGE("video index not find");
        return;
    }

    // 5. 获取解码器
    AVCodecParameters *aVCodecParameters = avFormatContext->streams[video_index]->codecpar;
    AVCodec *decoder = avcodec_find_decoder(aVCodecParameters->codec_id);
    AVCodecContext *avcodec_context = avcodec_alloc_context3(decoder);

    if (avcodec_parameters_to_context(avcodec_context, aVCodecParameters) < 0) {
        LOGE("avcodec_parameters_to_context error");
        return;
    }

    // 6. 打开解码器
    if (avcodec_open2(avcodec_context, decoder, NULL) != 0) {
        LOGE("avcodec_open2 error");
        return;
    }

    //绘制窗口 从 java 层拿到 surface ，在通过 surface 拿到 native_window，绘制画面到 window 窗口上
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    //窗口缓冲区
    ANativeWindow_Buffer outBuffer;

    //设置缓冲区的属性（宽、高、像素格式）
    ANativeWindow_setBuffersGeometry(nativeWindow, avcodec_context->width,
                                     avcodec_context->height,
                                     WINDOW_FORMAT_RGBA_8888);

    //解码上下文 指定播放的是 argb8888 格式 ，宽高等属性和输入视频一致
    SwsContext *sws_ctx = sws_getContext(
            avcodec_context->width, avcodec_context->height, avcodec_context->pix_fmt,
            avcodec_context->width, avcodec_context->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, 0, 0, 0);

    // 7. 一帧一帧解码播放
    AVPacket *packet = av_packet_alloc();
    AVFrame *avFrame;
    //从 ffmpeg 上下文中拿到 AVPacket 数据
    while (av_read_frame(avFormatContext, packet) >= 0) {
        LOGE("av_read_frame");

        if (packet->stream_index != video_index) {
            continue;
        }
        avFrame = av_frame_alloc();
        //解码 AVPacket->AVFrame
        ret = avcodec_send_packet(avcodec_context, packet);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF) || (ret == AVERROR(ENOMEM))) {
            break;
        } else if (ret < 0) {
            LOGE("legitimate decoding 1 errors");
            break;
        }
        ret = avcodec_receive_frame(avcodec_context, avFrame);
        // 这一帧数据是坏的
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            break;
        }
        uint8_t *dst_data[0];
        int dst_linesize[0];
        // 一帧视频的大小
        av_image_alloc(dst_data, dst_linesize,
                       avcodec_context->width, avcodec_context->height,
                       AV_PIX_FMT_RGBA, 1);


        if (ret == 0) {
            // lock native window 相当于锁住画布 outBuffer 为渲染一帧的缓冲区
            ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
            // 数据：avFrame -> dst_data
            sws_scale(sws_ctx,
                      reinterpret_cast<const uint8_t *const *>(avFrame->data), avFrame->linesize, 0,
                      avFrame->height,
                      dst_data, dst_linesize);

            uint8_t *dst = (uint8_t *) outBuffer.bits;
            int destStride = outBuffer.stride * 4;
            uint8_t *src_data = dst_data[0];
            int src_linesize = dst_linesize[0];
            uint8_t *firstWindown = static_cast<uint8_t *>(outBuffer.bits);
            // 拷贝数据， 数据： dst_data -> outBuffer
            for (int i = 0; i < outBuffer.height; ++i) {
                memcpy(firstWindown + i * destStride, src_data + i * src_linesize, destStride);
            }

            //提交画布数据
            ANativeWindow_unlockAndPost(nativeWindow);
            usleep(1000 * 16);
            av_frame_free(&avFrame);
        }


    }

    // 释放资源
    ANativeWindow_release(nativeWindow);
    avcodec_close(avcodec_context);
    avformat_free_context(avFormatContext);
    env->ReleaseStringUTFChars(path, _path);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_VideoControl_native_1audio_1init(JNIEnv *env, jclass clazz,
                                                                jstring input_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    //输出 pcm 文件的路径
    const char *output = "/storage/emulated/0/aa.pcm";
    int ret = -1;

    av_register_all();

    avformat_network_init();

    AVFormatContext *avFormatContext = avformat_alloc_context();

    AVDictionary *options = NULL;

    if (av_dict_set(&options, "timeout", "3000000", 0) < 0) {
        LOGE("av_dict_set error");
        return;
    }


    if ((ret = avformat_open_input(&avFormatContext, input, NULL, &options)) != 0) {
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("avformat_open_input error Couldn’t open file %s: %d(%s)", input, ret, buf);
        return;
    }

    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("avformat_find_stream_info error");
        return;
    }

    int audio_index = -1;
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
        }
    }

    if (audio_index == -1) {
        LOGE("audio index not find");
        return;
    }

    AVCodecParameters *avCodecParameters = avFormatContext->streams[audio_index]->codecpar;
    AVCodec *avCodec = avcodec_find_decoder(avCodecParameters->codec_id);
    AVCodecContext *avCodecContext = avcodec_alloc_context3(avCodec);
    if (avcodec_parameters_to_context(avCodecContext, avCodecParameters) < 0) {
        LOGE("avcodec_parameters_to_context error");
        return;
    }

    if (avcodec_open2(avCodecContext, avCodec, NULL) != 0) {
        LOGE("avcodec_open2 error");
        return;
    }


    int64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    enum AVSampleFormat out_sample_fmt = AVSampleFormat::AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int64_t in_ch_layout = avCodecContext->channel_layout;
    enum AVSampleFormat in_sample_fmt = avCodecContext->sample_fmt;
    int in_sample_rate = avCodecContext->sample_rate;
    //拿到转换上下文 设置输入输出参数
    SwrContext *swrContext = swr_alloc_set_opts(NULL,
                                                out_ch_layout, out_sample_fmt, out_sample_rate,
                                                in_ch_layout, in_sample_fmt, in_sample_rate,
                                                0, NULL);
//    SwrContext *swrContext = swr_alloc_set_opts(NULL,
//                                                AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, avCodecContext->sample_rate,
//                                                avCodecContext->channel_layout, avCodecContext->sample_fmt, avCodecContext->sample_rate,
//                                                NULL, NULL);
    if (swr_init(swrContext) < 0) {
        LOGE("swr_init error");
        return;
    }
    AVPacket *avPacket = av_packet_alloc();
    AVFrame *avFrame;
    /*
     * 输出文件缓冲区
     *
     * 2 : 16bit 2个字节
     * 2 : 双通道  AV_CH_LAYOUT_STEREO = (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
     * 44100 : 采样率
     */
    uint8_t *out_buffer = (uint8_t *) (av_malloc(2 * 2 * 44100));
    //PCM数据输出的文件
    FILE *file = fopen(output, "wb");
    while (av_read_frame(avFormatContext, avPacket) >= 0) {
        //这里要先判断 之前在拿到 frame 之后判断 老是报 -22 错误
        if (avPacket->stream_index != audio_index) {
            continue;
        }
        LOGE("av_read_frame");

        ret = avcodec_send_packet(avCodecContext, avPacket);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF) || (ret == AVERROR(ENOMEM))) {
            break;
        } else if (ret < 0) {
            LOGE("legitimate decoding 1 errors ret=%d", ret);
            break;
        }


        avFrame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, avFrame);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF)) {
            break;
        } else if (ret < 0) {
            LOGE("legitimate decoding 2 errors");
            break;
        }

        //重采样 将 frame 数据写到输出缓冲区 out_buffer 里面
        ret = swr_convert(swrContext, &out_buffer, avFrame->nb_samples,
                          (const uint8_t **) avFrame->data, avFrame->nb_samples);

        //判断这个是因为之前我直接通过 swr_alloc_set_opts() 拿到了 SwrContext， 之后没有对 SwrContext 进行初始化 swr_init() 在 swr_convert() 报了一个 Invalid argument的错误
        if (ret < 0) {
            char buf[1024];
            av_strerror(ret, buf, 1024);
            LOGE("swr_convert error  %d(%s)", ret, buf);//Invalid argument
            continue;
        }

        //计算 out_buffer 数量
        int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        int data_size = ret * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        LOGE("data_size=%d", data_size);
        fwrite(out_buffer, 1, data_size, file);

//        ret = av_samples_get_buffer_size(NULL, channels, avFrame->format,
//                                         AV_SAMPLE_FMT_S16, 1);
//        fwrite(out_buffer, 1, ret, file);
    }

    //释放资源
    av_packet_free(&avPacket);
    av_free(avPacket);
    avPacket = NULL;
    fclose(file);
    av_free(out_buffer);
    out_buffer = NULL;
    swr_free(&swrContext);
    swrContext = NULL;
    avcodec_free_context(&avCodecContext);
    avCodecContext = NULL;
    avformat_close_input(&avFormatContext);
    avformat_free_context(avFormatContext);
    avFormatContext = NULL;
    env->ReleaseStringUTFChars(input_, input);
}
//缓冲器队列接口
SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
void *buffer;
FILE *pcmFile;
uint8_t *out_buffer;

int getPcmData(void **pcm) {
    int size = 0;
    while (!feof(pcmFile)) {
        size = fread(out_buffer, 1, 44100 * 2 * 2, pcmFile);
        if (out_buffer == NULL) {
            LOGE("%s", "read end");
            break;
        } else {
            LOGE("%s", "reading");
        }
        *pcm = out_buffer;
        //注意这儿的break
        break;
    }
    LOGE("size is %d", size);
    return size;
}

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context) {
    //assert(NULL == context);
    int size = getPcmData(&buffer);
    // for streaming playback, replace this test by logic to find and fill the next buffer
    if (NULL != buffer) {
        SLresult result;
        // enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer, size);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
    } else {
        LOGE("size null");
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_VideoControl_native_1opensl_1start(JNIEnv *env, jclass clazz) {

    const char *path = "/storage/emulated/0/aa.pcm";
    pcmFile = fopen(path, "r");

    SLresult result;
    SLObjectItf engineObj;
    SLEngineItf engineItf;

    //1.创建引擎
    result = slCreateEngine(&engineObj, 0, 0, 0, 0, 0);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    result = (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engineItf);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //2.创建混音器
    SLObjectItf mixerObj;
    SLOutputMixItf outputMixItf;
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

    result = (*mixerObj)->GetInterface(mixerObj, SL_IID_ENVIRONMENTALREVERB,&outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }

    //3.创建播放器
    SLObjectItf playerObj;
    SLPlayItf playItf;

    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};
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

    result = (*engineItf)->CreateAudioPlayer(engineItf, &playerObj, &pAudioSrc, &pAudioSnk,numInterfaces,
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

    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, pcmBufferCallBack, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //获取音量接口
    result = (*playerObj)->GetInterface(playerObj, SL_IID_VOLUME, &pcmPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //5.设置播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

    //6.回调
    pcmBufferCallBack(bqPlayerBufferQueue, NULL);

}

////缓冲器队列接口
//SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
////缓冲 buffer
//void *buffer;
//uint8_t *out_buffer;

AVFormatContext *avFormatContext;
int audio_index;
AVCodecParameters *avCodecParameters;
AVCodec *avCodec;
AVCodecContext *avCodecContext;
SwrContext *swrContext;
AVPacket *avPacket;
AVFrame *avFrame;
int ret = -1;


int getPackageSize(void **pcm) {
    avPacket = av_packet_alloc();
    int data_size = 0;
    while (av_read_frame(avFormatContext, avPacket) >= 0) {
        //这里要先判断 之前在拿到 frame 之后判断 老是报 -22 错误
        if (avPacket->stream_index != audio_index) {
            continue;
        }
        LOGE("av_read_frame");

        ret = avcodec_send_packet(avCodecContext, avPacket);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF) || (ret == AVERROR(ENOMEM))) {
            break;
        } else if (ret < 0) {
            LOGE("legitimate decoding 1 errors ret=%d", ret);
            break;
        }


        avFrame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, avFrame);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF)) {
            break;
        } else if (ret < 0) {
            LOGE("legitimate decoding 2 errors");
            break;
        }

        //重采样 将 frame 数据写到输出缓冲区 out_buffer 里面
        ret = swr_convert(swrContext, &out_buffer, avFrame->nb_samples,
                          (const uint8_t **) avFrame->data, avFrame->nb_samples);

        //判断这个是因为之前我直接通过 swr_alloc_set_opts() 拿到了 SwrContext， 之后没有对 SwrContext 进行初始化 swr_init() 在 swr_convert() 报了一个 Invalid argument的错误
        if (ret < 0) {
            char buf[1024];
            av_strerror(ret, buf, 1024);
            LOGE("swr_convert error  %d(%s)", ret, buf);//Invalid argument
            continue;
        }

        //计算 out_buffer 数量
        int out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        data_size = ret * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        *pcm = out_buffer;
//        LOGE("data_size=%d", data_size);
//        fwrite(out_buffer, 1, data_size, file);

    }
    return data_size;
}

void packageBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context) {
    //assert(NULL == context);
    int size = getPackageSize(&buffer);
    if (NULL != buffer) {
        SLresult result;
        // enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer, size);
    } else {
        LOGE("size null");
    }

}


void init_opensl_es() {

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
    SLPlayItf playItf;

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

    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, packageBufferCallBack, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //获取音量接口
    result = (*playerObj)->GetInterface(playerObj, SL_IID_VOLUME, &pcmPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    //5.设置播放状态
    (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);

    //6.回调
    packageBufferCallBack(bqPlayerBufferQueue, NULL);
}


extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_VideoControl_native_1audio_1play(JNIEnv *env, jclass clazz,jstring _path) {
    const char *path = env->GetStringUTFChars(_path, 0);

    av_register_all();

    avformat_network_init();

    avFormatContext = avformat_alloc_context();

    AVDictionary *options = NULL;

    if (av_dict_set(&options, "timeout", "3000000", 0) < 0) {
        LOGE("av_dict_set error");
        return;
    }


    if ((ret = avformat_open_input(&avFormatContext, path, NULL, &options)) != 0) {
        char buf[1024];
        av_strerror(ret, buf, 1024);
        LOGE("avformat_open_input error Couldn’t open file %s: %d(%s)", path, ret, buf);
        return;
    }

    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("avformat_find_stream_info error");
        return;
    }

    audio_index = -1;
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
        }
    }

    if (audio_index == -1) {
        LOGE("audio index not find");
        return;
    }

    avCodecParameters = avFormatContext->streams[audio_index]->codecpar;
    avCodec = avcodec_find_decoder(avCodecParameters->codec_id);
    avCodecContext = avcodec_alloc_context3(avCodec);
    if (avcodec_parameters_to_context(avCodecContext, avCodecParameters) < 0) {
        LOGE("avcodec_parameters_to_context error");
        return;
    }

    if (avcodec_open2(avCodecContext, avCodec, NULL) != 0) {
        LOGE("avcodec_open2 error");
        return;
    }


    int64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    enum AVSampleFormat out_sample_fmt = AVSampleFormat::AV_SAMPLE_FMT_S16;
    int out_sample_rate = 44100;
    int64_t in_ch_layout = avCodecContext->channel_layout;
    enum AVSampleFormat in_sample_fmt = avCodecContext->sample_fmt;
    int in_sample_rate = avCodecContext->sample_rate;
    //拿到转换上下文 设置输入输出参数
    swrContext = swr_alloc_set_opts(NULL,
                                    out_ch_layout, out_sample_fmt, out_sample_rate,
                                    in_ch_layout, in_sample_fmt, in_sample_rate,
                                    0, NULL);
    if (swr_init(swrContext) < 0) {
        LOGE("swr_init error");
        return;
    }


    /*
     * 输出文件缓冲区
     *
     * 2 : 16bit 2个字节
     * 2 : 双通道  AV_CH_LAYOUT_STEREO = (AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT)
     * 44100 : 采样率
     */
    out_buffer = (uint8_t *) (av_malloc(2 * 2 * 44100));
    init_opensl_es();
}
