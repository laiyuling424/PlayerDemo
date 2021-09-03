//
// Created by 赖於领 on 2021/9/3.
//

#include "VideoChannel.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:28 下午
 */
void *pthread_decode(void *context) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(context);
    videoChannel->decodePacket();
    return 0;
}

void *pthread_play(void *context) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(context);
    videoChannel->render();
    return 0;
}


VideoChannel::VideoChannel(AVCodecContext *context, AVRational time, int id, JavaCallHelper *javaCallHelper)
        : BaseChannel(context, time, id, javaCallHelper) {

}

VideoChannel::~VideoChannel() {

}

void VideoChannel::play() {
    this->getPacketQueue().setWork(1);
    this->getFrameQueue().setWork(1);
    this->setIsPlaying();
    pthread_create(&decode_pid, NULL, pthread_decode, this);
    pthread_create(&play_pid, NULL, pthread_play, this);
}

void VideoChannel::decodePacket() {
    int ret;
    AVPacket *packet = 0;
    while (this->getIsPlaying()) {
        ret = package_queue.pop(packet);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        ret = avcodec_send_packet(avCodecContext, packet);
        releaseAvPacket(packet);
        AVFrame *avFrame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, avFrame);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if ((ret == AVERROR(EINVAL)) || (ret == AVERROR_EOF)) {
            break;
        } else if (ret < 0) {
            LOGE("legitimate decoding 2 errors");
            break;
        }

        frame_queue.push(avFrame);
        while (frame_queue.size() > 100 && isPlaying) {
            av_usleep(1000 * 10);
            continue;
        }
    }
    releaseAvPacket(packet);
}

void VideoChannel::render() {
    while (this->getIsPlaying()) {

    }
}

void VideoChannel::realRender() {

}

void VideoChannel::stop() {

}

void VideoChannel::seek(int time) {

}

void VideoChannel::speed(int s) {

}

void VideoChannel::setSurface(jobject surface) {
    this->surface = surface;
}






