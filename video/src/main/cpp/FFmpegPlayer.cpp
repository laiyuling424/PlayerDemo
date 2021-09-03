//
// Created by 赖於领 on 2021/9/3.
//

#include "FFmpegPlayer.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:30 下午
 */

void *pthread_prepare(void *context) {
    FFmpegPlayer *player = static_cast<FFmpegPlayer *>(context);
    player->prepareFFmpeg();
    return 0;
}

void *pthread_play(void *context) {
    FFmpegPlayer *player = static_cast<FFmpegPlayer *>(context);
    player->play();
    return 0;
}

FFmpegPlayer::FFmpegPlayer(JavaVM *javaVM, jobject jclass) : javaVM(javaVM) {
    javaCallHelper = new JavaCallHelper(this->javaVM, jclass);
}


FFmpegPlayer::~FFmpegPlayer() {

}

void FFmpegPlayer::setPath(char *path) {
    this->url = path;
}

void FFmpegPlayer::setSurface(jobject surface) {
    this->surface = surface;
}


void FFmpegPlayer::prepare() {
    pthread_create(&prepare_pid, NULL, pthread_prepare, this);
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
            videoChannel = new VideoChannel(avCodecContext, this->formatContext->streams[i]->time_base, i,
                                            this->javaCallHelper);
            videoChannel->setSurface(this->surface);
            //视频
            //int fps = frame_rate.num / (double)frame_rate.den;
//            int fps =av_q2d(this->formatContext->streams[i]->avg_frame_rate);
        } else if (this->formatContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {

        }
    }
}


void FFmpegPlayer::start() {
    this->isPlaying = 1;
    if (videoChannel) {
        videoChannel->play();
    }
    pthread_create(&play_pid, NULL, pthread_play, this);
}

void FFmpegPlayer::play() {
    int ret = 0;
    while (this->isPlaying) {
        if (videoChannel && videoChannel->getPacketQueue().size() > 100) {
            av_usleep(1000 * 10);
            continue;
        }
        AVPacket *avPacket = av_packet_alloc();
        ret = av_read_frame(this->formatContext, avPacket);

        if (ret == 0) {
            if (avPacket->stream_index == videoChannel->getChannelId()) {
                videoChannel->getPacketQueue().push(avPacket);
            }
        } else if (ret == AVERROR_EOF) {
            //读取完毕 但是不一定播放完毕
            if (videoChannel->getPacketQueue().empty() && videoChannel->getFrameQueue().empty()) {
                LOGE("播放完毕。。。");
                break;
            }
            //因为seek 的存在，就算读取完毕，依然要循环 去执行av_read_frame(否则seek了没用...)
        } else {
            break;
        }
    }
    isPlaying = 0;
    videoChannel->stop();

}

void FFmpegPlayer::stop() {

}

void FFmpegPlayer::seek(int time) {

}

void FFmpegPlayer::error(int code, char *message) {

}






