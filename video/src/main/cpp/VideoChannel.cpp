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
    videoChannel->decodeVideoPacket();
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

void VideoChannel::decodeVideoPacket() {
    int ret;
    AVPacket *packet = 0;
    while (this->isPlaying) {
        ret = package_queue.pop(packet);
//        LOGE("VideoChannel package_queue pop size is %d", package_queue.size());
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        avcodec_send_packet(avCodecContext, packet);
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
//        LOGE("VideoChannel frame_queue push size is %d", frame_queue.size());
        while (frame_queue.size() > 100 && isPlaying) {
            av_usleep(1000 * 10);
            continue;
        }
    }
    releaseAvPacket(packet);
    LOGE("VideoChannel decodeVideoPacket thread end");
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
//        LOGE("VideoChannel frame_queue pop size is %d", frame_queue.size());
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }

        // 数据：avFrame -> dst_data
        sws_scale(sws_ctx,
                  reinterpret_cast<const uint8_t *const *>(avFrame->data), avFrame->linesize, 0,
                  avFrame->height,
                  dst_data, dst_linesize);

        renderFrame(avCodecContext->width, avCodecContext->height, dst_data[0], dst_linesize[0]);
//        LOGE("解码一帧视频  %d", frame_queue.size());
        clock = avFrame->pts * av_q2d(time_base);
        //延时的来源 解码延时 播放延时
        //解码时间 看注释 extra_delay = repeat_pict / (2*fps)
        double delay = avFrame->repeat_pict / (2 * fps);
        double audioClock = audioChannel->clock;
        double diff = clock - audioClock - fastTime;
//        LOGE("diff is %f,delay is %f", diff, delay);
        if (clock > audioClock) {//视频在前
            if (diff > 1) {//差的太多，睡双倍
                av_usleep((delay * 2) * 1000000);
            } else {//差的不多，延时
                av_usleep((delay + diff) * 1000000);
            }
        } else {//音频在前
            if (diff > 1) {//不休眠

            } else if (diff >= 0.05) {//差的不多 视频需要丢包 同步
                releaseAvFrame(avFrame);
                AVFrame *frame;
                if (frame_queue.pop(frame)) {
                    releaseAvFrame(frame);
                }
            } else { //差不多 可以选择性的丢包 删除 key frame

            }
        }
        releaseAvFrame(avFrame);
    }

    //播放完了
    av_freep(&dst_data[0]);
    isPlaying = false;
    releaseAvFrame(avFrame);
    sws_freeContext(sws_ctx);
    LOGE("VideoChannel render thread end");
}

void VideoChannel::play() {
    this->package_queue.setWork(true);
    this->frame_queue.setWork(true);
    this->isPlaying = true;
    pthread_create(&decode_pid, NULL, pthread_video_decode, this);
    pthread_create(&play_pid, NULL, pthread_video_play, this);
    pthread_detach(play_pid);
    pthread_detach(decode_pid);
}

void VideoChannel::pause() {
    isPlaying = false;
//    this->package_queue.setWork(false);
//    this->frame_queue.setWork(false);
}

void VideoChannel::resume() {
    play();
}

void VideoChannel::stop() {
    isPlaying = false;
    this->package_queue.setWork(false);
    this->frame_queue.setWork(false);
    package_queue.clear();
    frame_queue.clear();
}

void VideoChannel::seek(int time) {
    /**
     * TODO 晚上说先 avformat_seek_file 再 avcodec_flush_buffers
     */
//    avcodec_flush_buffers(avCodecContext);
    package_queue.clear();
    frame_queue.clear();
}

void VideoChannel::speed(int s) {

}

void VideoChannel::setFrameRender(RenderFrame renderFrame) {
    this->renderFrame = renderFrame;
}

void VideoChannel::setAudioChannel(AudioChannel *audioChannel) {
    this->audioChannel = audioChannel;
}

void VideoChannel::setFps(int fps) {
    this->fps = fps;
}

void VideoChannel::audioTimeAdd(int time) {
    fastTime = time;
}









