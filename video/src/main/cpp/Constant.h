//
// Created by 赖於领 on 2021/9/3.
//

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 2:56 下午
 */
#ifndef LYLPLAYERDEMO_CONSTANT_H
#define LYLPLAYERDEMO_CONSTANT_H

#include <android/log.h>

#define TAG "lyll"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

#define THREAD_MAIN 1
#define THREAD_CHILD 2

//错误代码

//打不开视频
#define FFMPEG_CAN_NOT_OPEN_URL 1
//找不到流媒体
#define FFMPEG_CAN_NOT_FIND_STREAMS 2
//找不到解码器
#define FFMPEG_FIND_DECODER_FAIL 3
//无法根据解码器创建上下文
#define FFMPEG_ALLOC_CODEC_CONTEXT_FAIL 4
//根据流信息 配置上下文参数失败
#define FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL 6
//打开解码器失败
#define FFMPEG_OPEN_DECODER_FAIL 7
//没有音视频
#define FFMPEG_NOMEDIA 8
//初始化avFormatContext参数错误
#define AV_DICT_SET_ERROR 9


//视频的状态
#define PLAYER_NONE 0
#define PLAYER_PREPARE 1
#define PLAYER_PLAYING 2
#define PLAYER_STOP 3
#define PLAYER_DESTORY 4
#define PLAYER_ACTIONDO 5
#define PLAYER_PAUSE 6


#endif //LYLPLAYERDEMO_CONSTANT_H
