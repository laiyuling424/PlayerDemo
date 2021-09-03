//
// Created by 赖於领 on 2021/9/3.
//

#include "JavaCallHelper.h"

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 3:05 下午
 */
JavaCallHelper::JavaCallHelper(JavaVM *javaVM, jobject jclass) : javaVM(javaVM), playerControlclass(jclass) {

}

JavaCallHelper::~JavaCallHelper() {

}

void JavaCallHelper::call_java_videoInfo(int thread, int fps, int time) {

}

void JavaCallHelper::call_java_status(int thread, int status) {

}

void JavaCallHelper::call_java_ready(int thread, int alltime) {

}

void JavaCallHelper::call_java_error(int thread, int code, char *message) {

}
