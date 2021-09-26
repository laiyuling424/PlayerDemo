#include <jni.h>
#include <string>
#include <android/native_window_jni.h> // window 头文件 渲染画面数据
#include <zconf.h>
#include <android/log.h>  //log 打印类头文件
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <assert.h>

#include "lock_queue.h"
#include "Constant.h"
#include "BaseChannel.h"
#include "FFmpegPlayer.h"

//ffmpeg 是 C 写的，所以要以 C 的方式引入
extern "C" {
#include "libavcodec/avcodec.h"
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}




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
//缓冲 buffer
void *buffer;
uint8_t *out_buffer;
FILE *pcmFile;


int getPcmData(void **pcm) {
    int size = 0;
    while (!feof(pcmFile)) {
        //从 PCM 文件读取大小为1 数量为 44100 * 2 * 2 个数据并存入 out_buffer 中
        //size 为实际读的数量
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
    if (NULL != buffer) {
        SLresult result;
        // enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer, size);
    } else {
        LOGE("size null");
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_VideoControl_native_1opensl_1start(JNIEnv *env, jclass clazz) {

    const char *path = "/storage/emulated/0/aa.pcm";
    pcmFile = fopen(path, "r");
    if (pcmFile == NULL) {
        LOGE("%s", "open file error");
        return;
    }
    //44100 * 2 * 2 是上次输出 PCM 文件的格式， 44100=采样 2=左右声道 2=16 bit=2 byte
    out_buffer = (uint8_t *) malloc(44100 * 2 * 2);

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

    result = (*mixerObj)->GetInterface(mixerObj, SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }

    //3.创建播放器
    SLObjectItf playerObj;
    SLPlayItf playItf;

    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
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

    result = (*engineItf)->CreateAudioPlayer(engineItf, &playerObj, &pAudioSrc, &pAudioSnk, numInterfaces,
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
Java_com_dabaicai_video_ffmpeg_VideoControl_native_1audio_1play(JNIEnv *env, jclass clazz, jstring _path) {
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


//线程  ----》javaVM
JavaVM *javaVM = NULL;
//FFmpegPlayer *player;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVM = vm;
    return JNI_VERSION_1_4;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1prepare(JNIEnv *env, jobject thiz, jlong ptr, jstring path) {
    // TODO: implement native_prepare()
    const char *url = env->GetStringUTFChars(path, 0);
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    if (player) {
        player->setPath(url);
        player->prepare();
    }
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("native_1prepare thread_id is %d,process_id is %d", thread_id, process_id);
    env->ReleaseStringUTFChars(path, url);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1set_1surface(JNIEnv *env, jobject thiz, jobject surface) {
    //JavaVM *javaVM, JNIEnv *env, jobject jclass
    FFmpegPlayer *player = new FFmpegPlayer(javaVM, env, thiz);
    player->setSurface(surface);

    jclass _jclass = env->GetObjectClass(thiz);
    jfieldID ptr = env->GetFieldID(_jclass, "ptr", "J");
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("native_1set_1surface thread_id is %d,process_id is %d", thread_id, process_id);
    env->SetLongField(thiz, ptr, (jlong) player);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1start(JNIEnv *env, jobject thiz, jlong ptr) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("native_1start thread_id is %d,process_id is %d", thread_id, process_id);
    player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1stop(JNIEnv *env, jobject thiz, jlong ptr) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    player->stop();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1seek(JNIEnv *env, jobject thiz, jlong ptr, jint seektime) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    player->seek(seektime);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1speed(JNIEnv *env, jobject thiz, jlong ptr, jint speed) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    player->speed(speed);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1change_1path(JNIEnv *env, jobject thiz, jlong ptr, jstring path) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
//    player->setPath()
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1pause(JNIEnv *env, jobject thiz, jlong ptr) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    player->pause();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1resume(JNIEnv *env, jobject thiz, jlong ptr) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    player->resume();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_ffmpeg_PlayerControl_native_1audio_1time_1add(JNIEnv *env, jobject thiz, jlong ptr, jint time) {
    FFmpegPlayer *player = reinterpret_cast<FFmpegPlayer *>(ptr);
    player->audioTimeAdd(time);
}





//顶点着色器glsl的宏
// 第二个#号的意思是自动链接字符串，而不用增加引号，参考ijkplayer的写法

#define GET_STR(x) #x

static const char *vertexShader = GET_STR(

        attribute vec4 aPosition; //顶点坐标，在外部获取传递进来

        attribute vec2 aTexCoord; //材质（纹理）顶点坐标

        varying vec2 vTexCoord;   //输出的材质（纹理）坐标，给片元着色器使用
        void main() {
            //纹理坐标转换，以左上角为原点的纹理坐标转换成以左下角为原点的纹理坐标，
            // 比如以左上角为原点的（0，0）对应以左下角为原点的纹理坐标的（0，1）
            vTexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
            gl_Position = aPosition;
        }
);

//片元着色器,软解码和部分x86硬解码解码得出来的格式是YUV420p

static const char *fragYUV420P = GET_STR(

        precision mediump float;    //精度

        varying vec2 vTexCoord;     //顶点着色器传递的坐标，相同名字opengl会自动关联

        uniform sampler2D yTexture; //输入的材质（不透明灰度，单像素）

        uniform sampler2D uTexture;

        uniform sampler2D vTexture;
        void main() {
            vec3 yuv;
            vec3 rgb;
            yuv.r = texture2D(yTexture, vTexCoord).r; // y分量
            // 因为UV的默认值是127，所以我们这里要减去0.5（OpenGLES的Shader中会把内存中0～255的整数数值换算为0.0～1.0的浮点数值）
            yuv.g = texture2D(uTexture, vTexCoord).r - 0.5; // u分量
            yuv.b = texture2D(vTexture, vTexCoord).r - 0.5; // v分量
            // yuv转换成rgb，两种方法，一种是RGB按照特定换算公式单独转换
            // 另外一种是使用矩阵转换
            rgb = mat3(1.0, 1.0, 1.0,
                       0.0, -0.39465, 2.03211,
                       1.13983, -0.58060, 0.0) * yuv;
            //输出像素颜色
            gl_FragColor = vec4(rgb, 1.0);
        }
);

GLint InitShader(const char *code, GLint type) {
    //创建shader
    GLint sh = glCreateShader(type);
    if (sh == 0) {
        LOGE("glCreateShader %d failed!", type);
        return 0;
    }
    //加载shader
    glShaderSource(sh,
                   1,    //shader数量
                   &code, //shader代码
                   0);   //代码长度
    //编译shader
    glCompileShader(sh);

    //获取编译情况
    GLint status;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOGE("glCompileShader failed!");
        return 0;
    }
    LOGE("glCompileShader success!");
    return sh;
}


/**
 * 将数据转换成double类型的一个方法
 * @param r
 * @return
 */
static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0 : (double) r.num / (double) r.den;
}

//extern "C"
//JNIEXPORT void JNICALL
//Java_com_dabaicai_video_GLPlayerActivity_startplay(JNIEnv *env, jobject thiz,
//                                                   jstring video_path, jobject surface) {
//
//
//    const char *path = env->GetStringUTFChars(video_path, 0);
//
//    AVFormatContext *fmt_ctx;
//    // 初始化格式化上下文
//    fmt_ctx = avformat_alloc_context();
//
//    // 使用ffmpeg打开文件
//    int re = avformat_open_input(&fmt_ctx, path, nullptr, nullptr);
//    if (re != 0) {
//        LOGE("打开文件失败：%s", av_err2str(re));
//        return;
//    }
//
//    //探测流索引
//    re = avformat_find_stream_info(fmt_ctx, nullptr);
//
//    if (re < 0) {
//        LOGE("索引探测失败：%s", av_err2str(re));
//        return;
//    }
//
//    //寻找视频流索引
//    int v_idx = av_find_best_stream(
//            fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
//
//    if (v_idx == -1) {
//        LOGE("获取视频流索引失败");
//        return;
//    }
//    //解码器参数
//    AVCodecParameters *c_par;
//    //解码器上下文
//    AVCodecContext *cc_ctx;
//    //声明一个解码器
//    const AVCodec *codec;
//
//    c_par = fmt_ctx->streams[v_idx]->codecpar;
//
//    //通过id查找解码器
//    codec = avcodec_find_decoder(c_par->codec_id);
//
//    if (!codec) {
//
//        LOGE("查找解码器失败");
//        return;
//    }
//
//    //用参数c_par实例化编解码器上下文，，并打开编解码器
//    cc_ctx = avcodec_alloc_context3(codec);
//
//    // 关联解码器上下文
//    re = avcodec_parameters_to_context(cc_ctx, c_par);
//
//    if (re < 0) {
//        LOGE("解码器上下文关联失败:%s", av_err2str(re));
//        return;
//    }
//
//    //打开解码器
//    re = avcodec_open2(cc_ctx, codec, nullptr);
//
//    if (re != 0) {
//        LOGE("打开解码器失败:%s", av_err2str(re));
//        return;
//    }
//
//    // 获取视频的宽高,也可以通过解码器获取
//    AVStream *as = fmt_ctx->streams[v_idx];
//    int width = as->codecpar->width;
//    int height = as->codecpar->height;
//
//    LOGE("width:%d", width);
//    LOGE("height:%d", height);
//
//    //数据包
//    AVPacket *pkt;
//    //数据帧
//    AVFrame *frame;
//
//    //初始化
//    pkt = av_packet_alloc();
//    frame = av_frame_alloc();
//
//    //1 获取原始窗口
//    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
//
//
//    ////////////////////
//    ///EGL
//    //1 EGL display创建和初始化
//    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//    if (display == EGL_NO_DISPLAY) {
//        LOGE("eglGetDisplay failed!");
//        return;
//    }
//    if (EGL_TRUE != eglInitialize(display, 0, 0)) {
//        LOGE("eglInitialize failed!");
//        return;
//    }
//    //2 surface
//    //2-1 surface窗口配置
//    //输出配置
//    EGLConfig config;
//    EGLint configNum;
//    EGLint configSpec[] = {
//            EGL_RED_SIZE, 8,
//            EGL_GREEN_SIZE, 8,
//            EGL_BLUE_SIZE, 8,
//            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
//    };
//    if (EGL_TRUE != eglChooseConfig(display, configSpec, &config, 1, &configNum)) {
//        LOGE("eglChooseConfig failed!");
//        return;
//    }
//    //创建surface
//    EGLSurface winsurface = eglCreateWindowSurface(display, config, nwin, 0);
//    if (winsurface == EGL_NO_SURFACE) {
//        LOGE("eglCreateWindowSurface failed!");
//        return;
//    }
//
//    //3 context 创建关联的上下文
//    const EGLint ctxAttr[] = {
//            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
//    };
//    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
//    if (context == EGL_NO_CONTEXT) {
//        LOGE("eglCreateContext failed!");
//        return;
//    }
//    if (EGL_TRUE != eglMakeCurrent(display, winsurface, winsurface, context)) {
//        LOGE("eglMakeCurrent failed!");
//        return;
//    }
//
//    LOGE("EGL Init Success!");
//
//    //顶点和片元shader初始化
//    //顶点shader初始化
//    GLint vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
//    //片元yuv420 shader初始化
//    GLint fsh = InitShader(fragYUV420P, GL_FRAGMENT_SHADER);
//
//
//    /////////////////////////////////////////////////////////////
//    //创建渲染程序
//    GLint program = glCreateProgram();
//    if (program == 0) {
//        LOGE("glCreateProgram failed!");
//        return;
//    }
//    //渲染程序中加入着色器代码
//    glAttachShader(program, vsh);
//    glAttachShader(program, fsh);
//
//    //链接程序
//    glLinkProgram(program);
//    GLint status = 0;
//    glGetProgramiv(program, GL_LINK_STATUS, &status);
//    if (status != GL_TRUE) {
//        LOGE("glLinkProgram failed!");
//        return;
//    }
//    glUseProgram(program);
//    LOGE("glLinkProgram success!");
//    /////////////////////////////////////////////////////////////
//
//
//    //加入三维顶点数据 两个三角形组成正方形
//    static float vers[] = {
//            1.0f, -1.0f, 0.0f,
//            -1.0f, -1.0f, 0.0f,
//            1.0f, 1.0f, 0.0f,
//            -1.0f, 1.0f, 0.0f,
//    };
//    GLuint apos = (GLuint) glGetAttribLocation(program, "aPosition");
//    glEnableVertexAttribArray(apos);
//    //传递顶点
//    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 12, vers);
//
//    //加入材质坐标数据
//    static float txts[] = {
//            1.0f, 0.0f, //右下
//            0.0f, 0.0f,
//            1.0f, 1.0f,
//            0.0, 1.0
//    };
//    GLuint atex = (GLuint) glGetAttribLocation(program, "aTexCoord");
//    glEnableVertexAttribArray(atex);
//    glVertexAttribPointer(atex, 2, GL_FLOAT, GL_FALSE, 8, txts);
//
//    //材质纹理初始化
//    //设置纹理层
//    glUniform1i(glGetUniformLocation(program, "yTexture"), 0); //对于纹理第1层
//    glUniform1i(glGetUniformLocation(program, "uTexture"), 1); //对于纹理第2层
//    glUniform1i(glGetUniformLocation(program, "vTexture"), 2); //对于纹理第3层
//
//    //创建opengl纹理
//    GLuint texts[3] = {0};
//    //创建三个纹理
//    glGenTextures(3, texts);
//
//    //设置纹理属性
//    glBindTexture(GL_TEXTURE_2D, texts[0]);
//    //缩小的过滤器
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    //设置纹理的格式和大小
//    glTexImage2D(GL_TEXTURE_2D,
//                 0,           //细节基本 0默认
//                 GL_LUMINANCE,//gpu内部格式 亮度，灰度图
//                 width, height, //拉升到全屏
//                 0,             //边框
//                 GL_LUMINANCE,//数据的像素格式 亮度，灰度图 要与上面一致
//                 GL_UNSIGNED_BYTE, //像素的数据类型
//                 NULL                    //纹理的数据
//    );
//
//    //设置纹理属性
//    glBindTexture(GL_TEXTURE_2D, texts[1]);
//    //缩小的过滤器
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    //设置纹理的格式和大小
//    glTexImage2D(GL_TEXTURE_2D,
//                 0,           //细节基本 0默认
//                 GL_LUMINANCE,//gpu内部格式 亮度，灰度图
//                 width / 2, height / 2, //拉升到全屏
//                 0,             //边框
//                 GL_LUMINANCE,//数据的像素格式 亮度，灰度图 要与上面一致
//                 GL_UNSIGNED_BYTE, //像素的数据类型
//                 NULL                    //纹理的数据
//    );
//
//    //设置纹理属性
//    glBindTexture(GL_TEXTURE_2D, texts[2]);
//    //缩小的过滤器
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    //设置纹理的格式和大小
//    glTexImage2D(GL_TEXTURE_2D,
//                 0,           //细节基本 0默认
//                 GL_LUMINANCE,//gpu内部格式 亮度，灰度图
//                 width / 2, height / 2, //拉升到全屏
//                 0,             //边框
//                 GL_LUMINANCE,//数据的像素格式 亮度，灰度图 要与上面一致
//                 GL_UNSIGNED_BYTE, //像素的数据类型
//                 NULL                    //纹理的数据
//    );
//
//
//    //////////////////////////////////////////////////////
//    ////纹理的修改和显示
//    unsigned char *buf[3] = {0};
//    buf[0] = new unsigned char[width * height];
//    buf[1] = new unsigned char[width * height / 4];
//    buf[2] = new unsigned char[width * height / 4];
//
//
//    while (av_read_frame(fmt_ctx, pkt) >= 0) {//持续读帧
//
//        // 只解码视频流
//        if (pkt->stream_index == v_idx) {
//
//            //发送数据包到解码器
//            avcodec_send_packet(cc_ctx, pkt);
//
//            //清理
//            av_packet_unref(pkt);
//
//            //这里为什么要使用一个for循环呢？
//            // 因为avcodec_send_packet和avcodec_receive_frame并不是一对一的关系的
//            //一个avcodec_send_packet可能会出发多个avcodec_receive_frame
//            for (;;) {
//                // 接受解码的数据
//                re = avcodec_receive_frame(cc_ctx, frame);
//                LOGE("读取一帧");
//                if (re != 0) {
//                    break;
//                } else {
//
//                    // 解码得到YUV数据
//
//                    // 数据Y
//                    buf[0] = frame->data[0];
//
//                    memcpy(buf[0], frame->data[0], width * height);
//                    // 数据U
//                    memcpy(buf[1], frame->data[1], width * height / 4);
//
//                    // 数据V
//                    memcpy(buf[2], frame->data[2], width * height / 4);
//
//                    //激活第1层纹理,绑定到创建的opengl纹理
//                    glActiveTexture(GL_TEXTURE0);
//                    glBindTexture(GL_TEXTURE_2D, texts[0]);
//                    //替换纹理内容
//                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf[0]);
//
//
//                    //激活第2层纹理,绑定到创建的opengl纹理
//                    glActiveTexture(GL_TEXTURE0 + 1);
//                    glBindTexture(GL_TEXTURE_2D, texts[1]);
//                    //替换纹理内容
//                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf[1]);
//
//
//                    //激活第2层纹理,绑定到创建的opengl纹理
//                    glActiveTexture(GL_TEXTURE0 + 2);
//                    glBindTexture(GL_TEXTURE_2D, texts[2]);
//                    //替换纹理内容
//                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf[2]);
//
//                    //三维绘制
//                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//                    //窗口显示
//                    eglSwapBuffers(display, winsurface);
//                    av_usleep((unsigned int) (16 * 1000));
//                }
//            }
//        }
//    }
//    //关闭环境
//    avcodec_free_context(&cc_ctx);
//    // 释放资源
//    av_frame_free(&frame);
//    av_packet_free(&pkt);
//
//    avformat_free_context(fmt_ctx);
//
//    LOGE("播放完毕");
//
//    env->ReleaseStringUTFChars(video_path, path);
//
//}


//extern "C"
//JNIEXPORT void JNICALL
//Java_com_dabaicai_video_GLPlayerActivity_startplay(JNIEnv *env, jobject thiz,
//                                                   jstring video_path, jobject surface) {
//
//
//    const char *path = env->GetStringUTFChars(video_path, 0);
//
//    AVFormatContext *fmt_ctx;
//    // 初始化格式化上下文
//    fmt_ctx = avformat_alloc_context();
//
//    // 使用ffmpeg打开文件
//    int re = avformat_open_input(&fmt_ctx, path, nullptr, nullptr);
//    if (re != 0) {
//        LOGE("打开文件失败：%s", av_err2str(re));
//        return;
//    }
//
//    //探测流索引
//    re = avformat_find_stream_info(fmt_ctx, nullptr);
//
//    if (re < 0) {
//        LOGE("索引探测失败：%s", av_err2str(re));
//        return;
//    }
//
//    //寻找视频流索引
//    int v_idx = av_find_best_stream(
//            fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
//
//    if (v_idx == -1) {
//        LOGE("获取视频流索引失败");
//        return;
//    }
//    //解码器参数
//    AVCodecParameters *c_par;
//    //解码器上下文
//    AVCodecContext *cc_ctx;
//    //声明一个解码器
//    const AVCodec *codec;
//
//    c_par = fmt_ctx->streams[v_idx]->codecpar;
//
//    //通过id查找解码器
//    codec = avcodec_find_decoder(c_par->codec_id);
//
//    if (!codec) {
//
//        LOGE("查找解码器失败");
//        return;
//    }
//
//    //用参数c_par实例化编解码器上下文，，并打开编解码器
//    cc_ctx = avcodec_alloc_context3(codec);
//
//    // 关联解码器上下文
//    re = avcodec_parameters_to_context(cc_ctx, c_par);
//
//    if (re < 0) {
//        LOGE("解码器上下文关联失败:%s", av_err2str(re));
//        return;
//    }
//
//    //打开解码器
//    re = avcodec_open2(cc_ctx, codec, nullptr);
//
//    if (re != 0) {
//        LOGE("打开解码器失败:%s", av_err2str(re));
//        return;
//    }
//
//    // 获取视频的宽高,也可以通过解码器获取
//    AVStream *as = fmt_ctx->streams[v_idx];
//    int width = as->codecpar->width;
//    int height = as->codecpar->height;
//
//    LOGE("width:%d", width);
//    LOGE("height:%d", height);
//
//    //数据包
//    AVPacket *pkt;
//    //数据帧
//    AVFrame *frame;
//
//    //初始化
//    pkt = av_packet_alloc();
//    frame = av_frame_alloc();
//
//    //1 获取原始窗口
//    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
//
//
//    ////////////////////
//    ///EGL
//    //1 EGL display创建和初始化
//    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//    if (display == EGL_NO_DISPLAY) {
//        LOGE("eglGetDisplay failed!");
//        return;
//    }
//    if (EGL_TRUE != eglInitialize(display, 0, 0)) {
//        LOGE("eglInitialize failed!");
//        return;
//    }
//    //2 surface
//    //2-1 surface窗口配置
//    //输出配置
//    EGLConfig config;
//    EGLint configNum;
//    EGLint configSpec[] = {
//            EGL_RED_SIZE, 8,
//            EGL_GREEN_SIZE, 8,
//            EGL_BLUE_SIZE, 8,
//            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
//    };
//    if (EGL_TRUE != eglChooseConfig(display, configSpec, &config, 1, &configNum)) {
//        LOGE("eglChooseConfig failed!");
//        return;
//    }
//    //创建surface
//    EGLSurface winsurface = eglCreateWindowSurface(display, config, nwin, 0);
//    if (winsurface == EGL_NO_SURFACE) {
//        LOGE("eglCreateWindowSurface failed!");
//        return;
//    }
//
//    //3 context 创建关联的上下文
//    const EGLint ctxAttr[] = {
//            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
//    };
//    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
//    if (context == EGL_NO_CONTEXT) {
//        LOGE("eglCreateContext failed!");
//        return;
//    }
//    if (EGL_TRUE != eglMakeCurrent(display, winsurface, winsurface, context)) {
//        LOGE("eglMakeCurrent failed!");
//        return;
//    }
//
//    LOGE("EGL Init Success!");
//
//
//    const char* vertexShader = "attribute vec4 aPosition;\n"
//                         //                            "uniform mat4 uMatrix;\n"
//                         "attribute vec2 aCoordinate;\n"
//                         "varying vec2 vCoordinate;\n"
//                         "void main() {\n"
//                         //                            "  gl_Position = uMatrix*aPosition;\n"
//                         "  gl_Position = aPosition;\n"
//                         "  vCoordinate = aCoordinate;\n"
//                         "}";
//
//
//    const char* fragYUV420P = "precision mediump float;\n"
//                           "uniform sampler2D uTexture;\n"
//                           "varying vec2 vCoordinate;\n"
//                           "void main() {\n"
//                           "  vec4 color = texture2D(uTexture, vCoordinate);\n"
//                           //                            "  color.a = 0.5f;"
//                           //                            "  gl_FragColor = color;\n"
//                           "float gray = (color.r + color.g + color.b)/3.0;\n"
//                           "gl_FragColor = vec4(gray, gray, gray, 1.0);\n"
//                           //                            "  gl_FragColor = vec4(1, 1, 1, 1);\n"
//                           "}";
//
//    //顶点和片元shader初始化
//    //顶点shader初始化
//    GLint vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
//    //片元yuv420 shader初始化
//    GLint fsh = InitShader(fragYUV420P,  GL_FRAGMENT_SHADER);
//
//
//    /////////////////////////////////////////////////////////////
//    //创建渲染程序
//    GLint program = glCreateProgram();
//    if (program == 0) {
//        LOGE("glCreateProgram failed!");
//        return;
//    }
//    //渲染程序中加入着色器代码
//    glAttachShader(program, vsh);
//    glAttachShader(program, fsh);
//
//    //链接程序
//    glLinkProgram(program);
//    GLint status = 0;
//    glGetProgramiv(program, GL_LINK_STATUS, &status);
//    if (status != GL_TRUE) {
//        LOGE("glLinkProgram failed!");
//        return;
//    }
//    glUseProgram(program);
//    LOGE("glLinkProgram success!");
//    /////////////////////////////////////////////////////////////
//
//
//    /**上下颠倒的顶点矩阵*/
//    const GLfloat m_reserve_vertex_coors[8] = {
//            -1.0f, 1.0f,
//            1.0f, 1.0f,
//            -1.0f, -1.0f,
//            1.0f, -1.0f
//    };
//
//    const GLfloat m_vertex_coors[8] = {
//            -1.0f, -1.0f,
//            1.0f, -1.0f,
//            -1.0f, 1.0f,
//            1.0f, 1.0f
//    };
//
//    const GLfloat m_texture_coors[8] = {
//            0.0f, 1.0f,
//            1.0f, 1.0f,
//            0.0f, 0.0f,
//            1.0f, 0.0f
//    };
//
////    GLuint m_vertex_matrix_handler = glGetUniformLocation(program, "uMatrix");
//    GLuint m_vertex_pos_handler = glGetAttribLocation(program, "aPosition");
//    GLuint m_texture_handler = glGetUniformLocation(program, "uTexture");
//    GLuint m_texture_pos_handler = glGetAttribLocation(program, "aCoordinate");
//
//
//    GLuint m_texture_id = 0;
//    glGenTextures(1, &m_texture_id);
//
//    //激活指定纹理单元
//    glActiveTexture(GL_TEXTURE0);
//    //绑定纹理ID到纹理单元
//    glBindTexture(GL_TEXTURE_2D, m_texture_id);
//    //将活动的纹理单元传递到着色器里面
//    glUniform1i(m_texture_handler, 0);
//    //配置边缘过渡参数
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//
//
//    while (av_read_frame(fmt_ctx, pkt) >= 0) {//持续读帧
//
//        // 只解码视频流
//        if (pkt->stream_index == v_idx) {
//
//            //发送数据包到解码器
//            avcodec_send_packet(cc_ctx, pkt);
//
//            //清理
//            av_packet_unref(pkt);
//
//            //这里为什么要使用一个for循环呢？
//            // 因为avcodec_send_packet和avcodec_receive_frame并不是一对一的关系的
//            //一个avcodec_send_packet可能会出发多个avcodec_receive_frame
//            for (;;) {
//                // 接受解码的数据
//                re = avcodec_receive_frame(cc_ctx, frame);
//                LOGE("读取一帧");
//                if (re != 0) {
//                    break;
//                } else {
//
//
//                    glTexImage2D(GL_TEXTURE_2D, 0, // level一般为0
//                                 GL_RGBA, //纹理内部格式
//                                 width, height, // 画面宽高
//                                 0, // 必须为0
//                                 GL_RGBA, // 数据格式，必须和上面的纹理格式保持一直
//                                 GL_UNSIGNED_BYTE, // RGBA每位数据的字节数，这里是BYTE​: 1 byte
//                                 frame->data);// 画面数据
//
//
//
//                    //启用顶点的句柄
//                    glEnableVertexAttribArray(m_vertex_pos_handler);
//                    glEnableVertexAttribArray(m_texture_pos_handler);
//                    //设置着色器参数
////    glUniformMatrix4fv(m_vertex_matrix_handler, 1, false, m_matrix, 0);
//                    glVertexAttribPointer(m_vertex_pos_handler, 2, GL_FLOAT, GL_FALSE, 0, m_vertex_coors);
//                    glVertexAttribPointer(m_texture_pos_handler, 2, GL_FLOAT, GL_FALSE, 0, m_texture_coors);
//                    //开始绘制
//                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//
//                    //窗口显示
//                    eglSwapBuffers(display, winsurface);
//                    av_usleep((unsigned int) (16 * 1000));
//                }
//            }
//        }
//    }
//    //关闭环境
//    avcodec_free_context(&cc_ctx);
//    // 释放资源
//    av_frame_free(&frame);
//    av_packet_free(&pkt);
//
//    avformat_free_context(fmt_ctx);
//
//    LOGE("播放完毕");
//
//    env->ReleaseStringUTFChars(video_path, path);
//
//}

extern "C"
JNIEXPORT void JNICALL
Java_com_dabaicai_video_GLPlayerActivity_startplay(JNIEnv *env, jobject thiz,
                                                   jstring video_path, jobject surface) {


    const char *path = env->GetStringUTFChars(video_path, 0);

    AVFormatContext *fmt_ctx;
    // 初始化格式化上下文
    fmt_ctx = avformat_alloc_context();

    // 使用ffmpeg打开文件
    int re = avformat_open_input(&fmt_ctx, path, nullptr, nullptr);
    if (re != 0) {
        LOGE("打开文件失败：%s", av_err2str(re));
        return;
    }

    //探测流索引
    re = avformat_find_stream_info(fmt_ctx, nullptr);

    if (re < 0) {
        LOGE("索引探测失败：%s", av_err2str(re));
        return;
    }

    //寻找视频流索引
    int v_idx = av_find_best_stream(
            fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    if (v_idx == -1) {
        LOGE("获取视频流索引失败");
        return;
    }
    //解码器参数
    AVCodecParameters *c_par;
    //解码器上下文
    AVCodecContext *cc_ctx;
    //声明一个解码器
    const AVCodec *codec;

    c_par = fmt_ctx->streams[v_idx]->codecpar;

    //通过id查找解码器
    codec = avcodec_find_decoder(c_par->codec_id);

    if (!codec) {

        LOGE("查找解码器失败");
        return;
    }

    //用参数c_par实例化编解码器上下文，，并打开编解码器
    cc_ctx = avcodec_alloc_context3(codec);

    // 关联解码器上下文
    re = avcodec_parameters_to_context(cc_ctx, c_par);

    if (re < 0) {
        LOGE("解码器上下文关联失败:%s", av_err2str(re));
        return;
    }

    //打开解码器
    re = avcodec_open2(cc_ctx, codec, nullptr);

    if (re != 0) {
        LOGE("打开解码器失败:%s", av_err2str(re));
        return;
    }

    // 获取视频的宽高,也可以通过解码器获取
    AVStream *as = fmt_ctx->streams[v_idx];
    int width = as->codecpar->width;
    int height = as->codecpar->height;

    LOGE("width:%d", width);
    LOGE("height:%d", height);



    //1 获取原始窗口
//    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);

    // 绘制区域的宽高
    int32_t m_window_width = ANativeWindow_getWidth(nwin);
    int32_t m_window_height = ANativeWindow_getHeight(nwin);

    //设置宽高限制缓冲区中的像素数量
    ANativeWindow_setBuffersGeometry(nwin, m_window_width,
                                     m_window_height, WINDOW_FORMAT_RGBA_8888);

    glViewport(0, 0, m_window_width, m_window_height);


    ////////////////////
    ///EGL
    //1 EGL display创建和初始化
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed!");
        return;
    }
    if (EGL_TRUE != eglInitialize(display, 0, 0)) {
        LOGE("eglInitialize failed!");
        return;
    }
    //2 surface
    //2-1 surface窗口配置
    //输出配置
    EGLConfig config;
    EGLint configNum;
    EGLint configSpec[] = {EGL_BUFFER_SIZE, EGL_DONT_CARE,
                           EGL_RED_SIZE, 8,
                           EGL_GREEN_SIZE, 8,
                           EGL_BLUE_SIZE, 8,
                           EGL_ALPHA_SIZE, 8,
                           EGL_DEPTH_SIZE, 16,
                           EGL_STENCIL_SIZE, EGL_DONT_CARE,
                           EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                           EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                           EGL_NONE // the end
    };
    if (EGL_TRUE != eglChooseConfig(display, configSpec, &config, 1, &configNum)) {
        LOGE("eglChooseConfig failed!");
        return;
    }


    //3 context 创建关联的上下文
    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed!");
        return;
    }


    EGLint egl_format;
    EGLBoolean success = eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
    if (success != EGL_TRUE || eglGetError() != EGL_SUCCESS) {
        LOGE(TAG, "EGL get config fail");
        return;
    }


    //创建surface
    EGLSurface winsurface = eglCreateWindowSurface(display, config, nwin, 0);
    if (winsurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed!");
        return;
    }


    if (EGL_TRUE != eglMakeCurrent(display, winsurface, winsurface, context)) {
        LOGE("eglMakeCurrent failed!");
        return;
    }

    LOGE("EGL Init Success!");


    const char *vertexShader = "attribute vec4 aPosition;\n"
                               //                            "uniform mat4 uMatrix;\n"
                               "attribute vec2 aCoordinate;\n"
                               "varying vec2 vCoordinate;\n"
                               "void main() {\n"
                               //                            "  gl_Position = uMatrix*aPosition;\n"
                               "  gl_Position = aPosition;\n"
                               "  vCoordinate = aCoordinate;\n"
                               "}";

    const char *fragYUV420P = "precision mediump float;\n"
                              "uniform sampler2D uTexture;\n"
                              "varying vec2 vCoordinate;\n"
                              "void main() {\n"
                              //                              "  vec4 color = texture2D(uTexture, vCoordinate);\n"
                              //                            "  color.a = 0.5f;"
                              //                            "  gl_FragColor = color;\n"
                              //                              "float gray = (color.r + color.g + color.b)/3.0;\n"
                              //                              "gl_FragColor = vec4(gray, gray, gray, 1.0);\n"
                              "gl_FragColor = texture2D(uTexture, vCoordinate);\n"
                              //                            "  gl_FragColor = vec4(1, 1, 1, 1);\n"
                              "}";

    //顶点和片元shader初始化
    //顶点shader初始化
    GLint vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
    //片元yuv420 shader初始化
    GLint fsh = InitShader(fragYUV420P, GL_FRAGMENT_SHADER);


    /////////////////////////////////////////////////////////////
    //创建渲染程序
    GLint program = glCreateProgram();
    if (program == 0) {
        LOGE("glCreateProgram failed!");
        return;
    }
    //渲染程序中加入着色器代码
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);

    //链接程序
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        LOGE("glLinkProgram failed!");
        return;
    }
    glUseProgram(program);
    LOGE("glLinkProgram success!");
    /////////////////////////////////////////////////////////////


    /**上下颠倒的顶点矩阵*/
    const GLfloat m_reserve_vertex_coors[8] = {
            -1.0f, 1.0f,
            1.0f, 1.0f,
            -1.0f, -1.0f,
            1.0f, -1.0f
    };

    const GLfloat m_vertex_coors[8] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f
    };

    const GLfloat m_texture_coors[8] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f
    };

    GLuint m_vertex_pos_handler = glGetAttribLocation(program, "aPosition");
    GLuint m_texture_handler = glGetUniformLocation(program, "uTexture");
    GLuint m_texture_pos_handler = glGetAttribLocation(program, "aCoordinate");


    GLuint m_texture_id = 0;
    glGenTextures(1, &m_texture_id);

    //激活指定纹理单元
    glActiveTexture(GL_TEXTURE0);
    //绑定纹理ID到纹理单元
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    //将活动的纹理单元传递到着色器里面
    glUniform1i(m_texture_handler, 0);
    //配置边缘过渡参数
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);



    //数据包
    AVPacket *pkt;
    //数据帧
    AVFrame *frame;

    //初始化
    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    AVFrame *m_rgb_frame = NULL;
    m_rgb_frame = av_frame_alloc();
    SwsContext *m_sws_ctx = NULL;

    // 获取缓存大小
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_window_width, m_window_height, 1);
    // 分配内存
    uint8_t *m_buf_for_rgb_frame = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    // 将内存分配给RgbFrame，并将内存格式化为三个通道后，分别保存其地址
    av_image_fill_arrays(m_rgb_frame->data, m_rgb_frame->linesize,
                         m_buf_for_rgb_frame, AV_PIX_FMT_RGBA, m_window_width, m_window_height, 1);

    m_sws_ctx = sws_getContext(width, height, cc_ctx->pix_fmt,
                               m_window_width, m_window_height, AV_PIX_FMT_RGBA,
                               SWS_FAST_BILINEAR, NULL, NULL, NULL);


    while (av_read_frame(fmt_ctx, pkt) >= 0) {//持续读帧

        // 只解码视频流
        if (pkt->stream_index == v_idx) {

            //发送数据包到解码器
            avcodec_send_packet(cc_ctx, pkt);

            //清理
            av_packet_unref(pkt);

            //这里为什么要使用一个for循环呢？
            // 因为avcodec_send_packet和avcodec_receive_frame并不是一对一的关系的
            //一个avcodec_send_packet可能会出发多个avcodec_receive_frame
            for (;;) {
                // 接受解码的数据
                re = avcodec_receive_frame(cc_ctx, frame);
                LOGE("读取一帧");
                if (re != 0) {
                    break;
                } else {
//int sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
//              const int srcStride[], int srcSliceY, int srcSliceH,
//              uint8_t *const dst[], const int dstStride[])
                    sws_scale(m_sws_ctx, frame->data, frame->linesize, 0,
                              height, m_rgb_frame->data, m_rgb_frame->linesize);

                    glTexImage2D(GL_TEXTURE_2D, 0, // level一般为0
                                 GL_RGBA, //纹理内部格式
                                 m_window_width, m_window_height, // 画面宽高
                                 0, // 必须为0
                                 GL_RGBA, // 数据格式，必须和上面的纹理格式保持一直
                                 GL_UNSIGNED_BYTE, // RGBA每位数据的字节数，这里是BYTE​: 1 byte
                                 m_rgb_frame->data[0]);// 画面数据



                    //启用顶点的句柄
                    glEnableVertexAttribArray(m_vertex_pos_handler);
                    glEnableVertexAttribArray(m_texture_pos_handler);
                    //设置着色器参数
//    glUniformMatrix4fv(m_vertex_matrix_handler, 1, false, m_matrix, 0);
                    glVertexAttribPointer(m_vertex_pos_handler, 2, GL_FLOAT, GL_FALSE, 0, m_vertex_coors);
                    glVertexAttribPointer(m_texture_pos_handler, 2, GL_FLOAT, GL_FALSE, 0, m_texture_coors);
                    //开始绘制
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


                    //窗口显示
                    eglSwapBuffers(display, winsurface);
                    av_usleep((unsigned int) (16 * 1000));
                }
            }
        }
    }
    //关闭环境
    avcodec_free_context(&cc_ctx);
    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&pkt);

    avformat_free_context(fmt_ctx);

    LOGE("播放完毕");

    env->ReleaseStringUTFChars(video_path, path);

}

