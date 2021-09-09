//
// Created by 赖於领 on 2021/9/3.
//

#include "JavaCallHelper.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:05 下午
 */

bool isMain() {
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("thread_id is %d,process_id is %d", thread_id, process_id);
    if (thread_id == process_id) {
        return true;
    } else {
        return false;
    }
}

JavaCallHelper::JavaCallHelper(JavaVM *javaVM, JNIEnv *env, jobject jclazz) : javaVM(javaVM), env(env) {
    playerControlclass = env->NewGlobalRef(jclazz);
    jclass _jclass = env->GetObjectClass(playerControlclass);
    videoInfoID = env->GetMethodID(_jclass, "videoInfo", "(II)V");
    statusID = env->GetMethodID(_jclass, "status", "(I)V");
    readyID = env->GetMethodID(_jclass, "ready", "(I)V");
    errorID = env->GetMethodID(_jclass, "error", "(ILjava/lang/String;)V");
}

JavaCallHelper::~JavaCallHelper() {

}

void JavaCallHelper::call_java_videoInfo(int thread, int fps, int time) {
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("call_java_videoInfo thread is %d,fps is %d,time is %d", thread, fps, time);
    if (thread_id == process_id) {
        env->CallVoidMethod(playerControlclass, videoInfoID, fps, time);
    } else {
        JNIEnv *jniEnv;
        bool isAttached = false;
        if (javaVM->GetEnv((void **) &jniEnv, JNI_VERSION_1_4) != JNI_OK) {
            if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
                return;
            }
            isAttached = true;
        }
        jniEnv->CallVoidMethod(playerControlclass, videoInfoID, fps, time);
        if (isAttached) {
            javaVM->DetachCurrentThread();
        }
    }
}

void JavaCallHelper::call_java_status(int thread, int status) {
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("call_java_status thread is %d,status is %d,thread_id is %d,process_id is %d", thread, status, thread_id, process_id);
    if (thread_id == process_id) {
        env->CallVoidMethod(playerControlclass, statusID, status);
    } else {
        JNIEnv *jniEnv;
        bool isAttached = false;
        if (javaVM->GetEnv((void **) &jniEnv, JNI_VERSION_1_4) != JNI_OK) {
            if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
                return;
            }
            isAttached = true;
        }
        jniEnv->CallVoidMethod(playerControlclass, statusID, status);
        if (isAttached) {
            javaVM->DetachCurrentThread();
        }
    }

//    if (thread == THREAD_MAIN) {
//        env->CallVoidMethod(playerControlclass, statusID, status);
//    } else {
//        JNIEnv *jniEnv;
//        bool isAttached = false;
//        if (javaVM->GetEnv((void **) &jniEnv, JNI_VERSION_1_4) != JNI_OK) {
//            if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
//                return;
//            }
//            isAttached = true;
//        }
//        jniEnv->CallVoidMethod(playerControlclass, statusID, status);
//        if (isAttached) {
//            javaVM->DetachCurrentThread();
//        }
//    }

}

void JavaCallHelper::call_java_ready(int thread, int alltime) {
    pid_t thread_id = gettid();
    pid_t process_id = getpid();
    LOGE("call_java_ready thread is %d,alltime is %d,thread_id is %d,process_id is %d", thread, alltime, thread_id, process_id);
    if (thread_id == process_id) {
        env->CallVoidMethod(playerControlclass, readyID, alltime);
    } else {
        JNIEnv *jniEnv;
        bool isAttached = false;
        if (javaVM->GetEnv((void **) &jniEnv, JNI_VERSION_1_4) != JNI_OK) {
            if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
                return;
            }
            isAttached = true;
        }
        jniEnv->CallVoidMethod(playerControlclass, readyID, alltime);
        if (isAttached) {
            javaVM->DetachCurrentThread();
        }
    }

//    if (thread == THREAD_MAIN) {
//        env->CallVoidMethod(playerControlclass, readyID, alltime);
//    } else {
//        JNIEnv *jniEnv;
//        bool isAttached = false;
//        if (javaVM->GetEnv((void **) &jniEnv, JNI_VERSION_1_4) != JNI_OK) {
//            if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
//                return;
//            }
//            isAttached = true;
//        }
//        jniEnv->CallVoidMethod(playerControlclass, readyID, alltime);
//        if (isAttached) {
//            javaVM->DetachCurrentThread();
//        }
//    }
}

void JavaCallHelper::call_java_error(int thread, int code, char *message) {
    isMain();
    LOGE("call_java_error thread is %d,code is %d", thread, code);
    if (thread == THREAD_MAIN) {
        jstring _jstring = env->NewStringUTF(message);
        env->CallVoidMethod(playerControlclass, errorID, code, _jstring);
    } else {
        JNIEnv *jniEnv;
        bool isAttached = false;
        if (javaVM->GetEnv((void **) &jniEnv, JNI_VERSION_1_4) != JNI_OK) {
            if (javaVM->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
                return;
            }
            isAttached = true;
        }
        jstring _jstring = env->NewStringUTF(message);
        jniEnv->CallVoidMethod(playerControlclass, errorID, code, _jstring);
        if (isAttached) {
            javaVM->DetachCurrentThread();
        }
    }
}
