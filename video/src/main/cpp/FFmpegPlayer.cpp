//
// Created by 赖於领 on 2021/9/3.
//

#include "FFmpegPlayer.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:30 下午
 */
ANativeWindow *nativeWindow = 0;

void renderFrame(int32_t w, int32_t h, uint8_t *data, int linesize) {
    //窗口缓冲区
    ANativeWindow_Buffer outBuffer;

    //设置缓冲区的属性（宽、高、像素格式）
    ANativeWindow_setBuffersGeometry(nativeWindow, w, h, WINDOW_FORMAT_RGBA_8888);

    // lock native window 相当于锁住画布 outBuffer 为渲染一帧的缓冲区
    ANativeWindow_lock(nativeWindow, &outBuffer, NULL);


//    uint8_t *dst = (uint8_t *) outBuffer.bits;
    int destStride = outBuffer.stride * 4;
    uint8_t *src_data = data;
    int src_linesize = linesize;
    uint8_t *firstWindown = static_cast<uint8_t *>(outBuffer.bits);
    // 拷贝数据， 数据： dst_data -> outBuffer
    for (int i = 0; i < outBuffer.height; ++i) {
        memcpy(firstWindown + i * destStride, src_data + i * src_linesize, destStride);
    }

    //提交画布数据
    ANativeWindow_unlockAndPost(nativeWindow);
}

void *pthread_prepare(void *context) {
    FFmpegPlayer *player = static_cast<FFmpegPlayer *>(context);
    player->prepareFFmpeg();
    return 0;
}

void *pthread_ffmpeg_play(void *context) {
    FFmpegPlayer *player = static_cast<FFmpegPlayer *>(context);
    player->play();
    return 0;
}

FFmpegPlayer::FFmpegPlayer(JavaVM *javaVM, JNIEnv *env, jobject jclass) : javaVM(javaVM), env(env) {
    javaCallHelper = new JavaCallHelper(this->javaVM, env, jclass);
}


FFmpegPlayer::~FFmpegPlayer() {

}

void FFmpegPlayer::setPath(const char *path) {
    this->url = path;
}

void FFmpegPlayer::setSurface(jobject surface) {
    this->surface = surface;
    //绘制窗口 从 java 层拿到 surface ，在通过 surface 拿到 native_window，绘制画面到 window 窗口上
    nativeWindow = ANativeWindow_fromSurface(env, surface);
}


void FFmpegPlayer::prepare() {
    javaCallHelper->call_java_status(THREAD_MAIN, PLAYER_PREPARE);
    pthread_create(&prepare_pid, NULL, pthread_prepare, this);
    pthread_detach(prepare_pid);
}

void FFmpegPlayer::prepareFFmpeg() {
    // 1. 注册组件，就是一些初始化工作
    avformat_network_init();

    this->formatContext = avformat_alloc_context();
    //初始化 ffmpeg 参数
    AVDictionary *options = NULL;

    int ret = -1;

    if (av_dict_set(&options, "timeout", "3000000", 0) < 0) {
        error(AV_DICT_SET_ERROR, "av_dict_set error");
        LOGE("av_dict_set error");
        return;
    }

    //NULL 输入文件的封装格式,FFmpeg自动检测 AVInputFormat
    // 2. 打开视频文件
    if ((ret = avformat_open_input(&this->formatContext, this->url, NULL, &options)) != 0) {
        char buf[1024];
        av_strerror(ret, buf, 1024);
        error(FFMPEG_CAN_NOT_OPEN_URL, "avformat_open_input error");
        LOGE("avformat_open_input error Couldn’t open file %s: %d(%s)", this->url, ret, buf);
        return;
    }

    // 3. 获取视频信息
    if (avformat_find_stream_info(this->formatContext, NULL) < 0) {
        error(FFMPEG_CAN_NOT_FIND_STREAMS, "avformat_find_stream_info error");
        LOGE("avformat_find_stream_info error");
        return;
    }



    // 4. 查找视频流
    for (int i = 0; i < this->formatContext->nb_streams; ++i) {
        AVCodecParameters *avCodecParameters = this->formatContext->streams[i]->codecpar;
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
        if (this->formatContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            if (!videoChannel) {
                LOGE("videoChannel channel id is %d", i);
                videoChannel = new VideoChannel(avCodecContext, this->formatContext->streams[i]->time_base, i,
                                                this->javaCallHelper);
                videoChannel->setFrameRender(renderFrame);
                AVRational frame_rate = this->formatContext->streams[i]->avg_frame_rate;
                int fps = av_q2d(frame_rate);
                videoChannel->setFps(fps);
            }
        } else if (this->formatContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            if (!audioChannel) {
                LOGE("audioChannel channel id is %d", i);
                audioChannel = new AudioChannel(avCodecContext, this->formatContext->streams[i]->time_base, i,
                                                this->javaCallHelper);
            }
        }
    }
    if (!videoChannel && !audioChannel) {
        return;
    }
    if (videoChannel && audioChannel) {
        videoChannel->setAudioChannel(audioChannel);
        javaCallHelper->call_java_ready(THREAD_CHILD, formatContext->duration / AV_TIME_BASE);
    }
}


void FFmpegPlayer::start() {
    this->isPlaying = true;
    if (audioChannel) {
        audioChannel->play();
    }
    if (videoChannel) {
        videoChannel->play();
    }
    pthread_create(&play_pid, NULL, pthread_ffmpeg_play, this);
    pthread_detach(play_pid);
    javaCallHelper->call_java_status(THREAD_MAIN, PLAYER_PLAYING);
}

void FFmpegPlayer::play() {
    int ret = 0;
    while (this->isPlaying) {
        if (videoChannel && videoChannel->package_queue.size() > 100) {
            av_usleep(1000 * 10);
            continue;
        }
        if (audioChannel && audioChannel->package_queue.size() > 100) {
            av_usleep(1000 * 10);
            continue;
        }
        AVPacket *avPacket = av_packet_alloc();
        ret = av_read_frame(this->formatContext, avPacket);
//        LOGE("avPacket->stream_index is %d", avPacket->stream_index);
        if (ret == 0) {
            if (avPacket->stream_index == videoChannel->channelId) {
                videoChannel->package_queue.push(avPacket);
//                LOGE("videoChannel package_queue push size is %d", videoChannel->package_queue.size());
            } else if (avPacket->stream_index == audioChannel->channelId) {
                audioChannel->package_queue.push(avPacket);
//                LOGE("audioChannel package_queue push size is %d", audioChannel->package_queue.size());
            }
        } else if (ret == AVERROR_EOF) {
            //读取完毕 但是不一定播放完毕
            if (videoChannel->package_queue.empty() && videoChannel->frame_queue.empty() &&
                audioChannel->package_queue.empty() && audioChannel->frame_queue.empty()) {
                LOGE("播放完毕。。。");
                break;
            }
            //因为seek 的存在，就算读取完毕，依然要循环 去执行av_read_frame(否则seek了没用...)
        } else {
            break;
        }
    }

    //1.播放暂停
    //2.播放完了
    if (!isPause) {
        stop();
    }
    LOGE("FFmpegPlayer play thread end");
}

void FFmpegPlayer::stop() {
    isPlaying = false;
    videoChannel->stop();
    audioChannel->stop();
//    javaCallHelper->call_java_status(THREAD_MAIN, PLAYER_STOP);
}

void FFmpegPlayer::seek(int time) {
    if (!isPlaying) {
        return;
    }
    time = (time + 3) * AV_TIME_BASE;
    time += formatContext->start_time;
    LOGE("FFmpegPlayer::seek time is %d", time);
    if (avformat_seek_file(formatContext, -1, INT64_MIN, time, INT64_MAX, AVSEEK_FLAG_BACKWARD) < 0) {
        LOGE("avformat_seek_file error");
        return;
    }
    if (audioChannel) {
        audioChannel->seek(time);
    }
    if (videoChannel) {
        videoChannel->seek(time);
    }
}

void FFmpegPlayer::error(int code, char *message) {

}

void FFmpegPlayer::pause() {
    if (audioChannel && videoChannel && isPlaying) {
        isPlaying = false;
        isPause = true;
        audioChannel->pause();
        videoChannel->pause();
        javaCallHelper->call_java_status(THREAD_MAIN, PLAYER_PAUSE);
    }
}


void FFmpegPlayer::resume() {
    if (audioChannel && videoChannel && !isPlaying) {
        isPlaying = true;
        isPause = false;
        if (audioChannel) {
            audioChannel->resume();
        }
        if (videoChannel) {
            videoChannel->resume();
        }
        pthread_create(&play_pid, NULL, pthread_ffmpeg_play, this);
        pthread_detach(play_pid);
        javaCallHelper->call_java_status(THREAD_MAIN, PLAYER_PLAYING);
    }
}

void FFmpegPlayer::speed(int speed) {

}

void FFmpegPlayer::audioTimeAdd(int time) {
    if (audioChannel) {
//        audioChannel->audioTimeAdd(time);
    }
}

void *release_thread(void *arg) {
//    FFmpegPlayer *player = static_cast<FFmpegPlayer *>(arg);
//    isPlaying = true;
//    nativeWindow = NULL;
//    pthread_join(prepare_pid, NULL);
//    pthread_join(play_pid, NULL);
//    if (audioChannel) {
//        audioChannel->release();
//        delete audioChannel;
//    }
//    if (videoChannel) {
//        videoChannel->release();
//        delete videoChannel;
//    }
    return 0;
}

void FFmpegPlayer::release() {
    //窗口及全局变量释放 队列清空
    //锁的释放 开辟的空间
    //线程释放前需要吧线程先关闭 pthread_join
    //要不要开线程来清理？  开线程的话 private 数据怎么处理？ 变public还是怎么说
//    pthread_create(&release_pid, NULL, release_thread, this);

    isPlaying = true;
    nativeWindow = NULL;
    pthread_join(prepare_pid, NULL);
    pthread_join(play_pid, NULL);
    if (audioChannel) {
        audioChannel->release();
        delete audioChannel;
    }
    if (videoChannel) {
        videoChannel->release();
        delete videoChannel;
    }
}











