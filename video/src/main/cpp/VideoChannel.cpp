//
// Created by 赖於领 on 2021/9/3.
//

#include "VideoChannel.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:28 下午
 */
void *pthread_video_decode(void *context) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(context);
    videoChannel->decodePacket();
    return 0;
}

void *pthread_video_play(void *context) {
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
    this->package_queue.setWork(true);
    this->frame_queue.setWork(true);
    this->setIsPlaying();
    pthread_create(&decode_pid, NULL, pthread_video_decode, this);
    pthread_create(&play_pid, NULL, pthread_video_play, this);
}

void VideoChannel::decodePacket() {
    int ret;
    AVPacket *packet = 0;
    while (this->getIsPlaying()) {
        ret = package_queue.pop(packet);
        LOGE("package_queue pop size is %d", package_queue.size());
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
        LOGE("frame_queue push size is %d", frame_queue.size());
        while (frame_queue.size() > 100 && isPlaying) {
            av_usleep(1000 * 10);
            continue;
        }
    }
    releaseAvPacket(packet);
}

void VideoChannel::render() {
    //解码上下文 指定播放的是 argb8888 格式 ，宽高等属性和输入视频一致
    SwsContext *sws_ctx = sws_getContext(
            avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
            avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, 0, 0, 0);

    uint8_t *dst_data[4];
    int dst_linesize[4];
    // 一帧视频的大小
    av_image_alloc(dst_data, dst_linesize,
                   avCodecContext->width, avCodecContext->height,
                   AV_PIX_FMT_RGBA, 1);
    int ret;
    AVFrame *avFrame = 0;
    while (isPlaying) {
        ret = frame_queue.pop(avFrame);
        LOGE("frame_queue pop size is %d", frame_queue.size());
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        if (ret == 1) {
            // 数据：avFrame -> dst_data
            sws_scale(sws_ctx,
                      reinterpret_cast<const uint8_t *const *>(avFrame->data), avFrame->linesize, 0,
                      avFrame->height,
                      dst_data, dst_linesize);

            renderFrame(avCodecContext->width, avCodecContext->height, dst_data[0], dst_linesize[0]);
        }
        LOGE("解码一帧视频  %d", frame_queue.size());
        releaseAvFrame(avFrame);
    }

    //播放完了
    av_freep(&dst_data[0]);
    isPlaying = false;
    releaseAvFrame(avFrame);
    sws_freeContext(sws_ctx);
}

void VideoChannel::stop() {

}

void VideoChannel::seek(int time) {

}

void VideoChannel::speed(int s) {

}

void VideoChannel::setFrameRender(RenderFrame renderFrame) {
    this->renderFrame = renderFrame;
}







