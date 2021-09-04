//
// Created by 赖於领 on 2021/9/3.
//

#include "JavaCallHelper.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:05 下午
 */
JavaCallHelper::JavaCallHelper(JavaVM *javaVM, JNIEnv *env, jobject jclazz) : javaVM(javaVM), env(env), playerControlclass(jclazz) {
    jclass _jclass = env->GetObjectClass(jclazz);
    videoInfoID = env->GetMethodID(_jclass, "videoInfo", "(II)V");
    statusID = env->GetMethodID(_jclass, "status", "(I)V");
    readyID = env->GetMethodID(_jclass, "ready", "(I)V");
    errorID = env->GetMethodID(_jclass, "error", "(ILjava/lang/String;)V");
}

JavaCallHelper::~JavaCallHelper() {

}

void JavaCallHelper::call_java_videoInfo(int thread, int fps, int time) {
    if (thread == THREAD_MAIN) {
        env->CallVoidMethod(playerControlclass, videoInfoID, fps, time);
    } else {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(playerControlclass, videoInfoID, fps, time);
        javaVM->DetachCurrentThread();
    }
}

void JavaCallHelper::call_java_status(int thread, int status) {
    if (thread == THREAD_MAIN) {
        env->CallVoidMethod(playerControlclass, statusID, status);
    } else {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(playerControlclass, statusID, status);
        javaVM->DetachCurrentThread();
    }
}

void JavaCallHelper::call_java_ready(int thread, int alltime) {
    if (thread == THREAD_MAIN) {
        env->CallVoidMethod(playerControlclass, readyID, alltime);
    } else {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(playerControlclass, readyID, alltime);
        javaVM->DetachCurrentThread();
    }
}

void JavaCallHelper::call_java_error(int thread, int code, char *message) {

    if (thread == THREAD_MAIN) {
        jstring _jstring = env->NewStringUTF(message);
        env->CallVoidMethod(playerControlclass, errorID, code, _jstring);
    } else {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
            return;
        }
        jstring _jstring = env->NewStringUTF(message);
        jniEnv->CallVoidMethod(playerControlclass, errorID, code, _jstring);
        javaVM->DetachCurrentThread();
    }
}
