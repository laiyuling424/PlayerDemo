//
// Created by 赖於领 on 2021/9/3.
//

#ifndef LYLPLAYERDEMO_JAVACALLHELPER_H
#define LYLPLAYERDEMO_JAVACALLHELPER_H

#include <jni.h>
#include "Constant.h"
#include <unistd.h>

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:05 下午
 */
class JavaCallHelper {
private:
    jobject playerControlclass;
    JavaVM *javaVM;
    JNIEnv *env;
    //private void videoInfo(int fps, int time)
    //private void status(int status)
    //private void ready(int alltime)
    //private void error(int code, String message)
    jmethodID videoInfoID;
    jmethodID statusID;
    jmethodID readyID;
    jmethodID errorID;

public:
    JavaCallHelper(JavaVM *javaVM, JNIEnv *env, jobject jclass);

    ~JavaCallHelper();

    void call_java_videoInfo(int thread, int fps, int time);

    void call_java_status(int thread, int status);

    void call_java_ready(int thread, int alltime);

    void call_java_error(int thread, int code, char *message);
};


#endif //LYLPLAYERDEMO_JAVACALLHELPER_H

