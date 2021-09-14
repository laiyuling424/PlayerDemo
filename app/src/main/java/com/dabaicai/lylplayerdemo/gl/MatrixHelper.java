package com.dabaicai.lylplayerdemo.gl;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/14 1:48 下午
 */
public class MatrixHelper {
    /**
     * @param m      生成的新矩阵
     * @param degree 视野角度
     * @param aspect 宽高比
     * @param n      到近处平面的距离
     * @param f      到远处平面的距离
     */
    public static void perspetiveM(float[] m, float degree, float aspect, float n, float f) {

        //计算焦距
        float angle = (float) (degree * Math.PI / 180.0);
        float a = (float) (1.0f / Math.tan(angle / 2.0));

        m[0] = a / aspect;
        m[1] = 0f;
        m[2] = 0f;
        m[3] = 0f;

        m[4] = 0f;
        m[5] = a;
        m[6] = 0f;
        m[7] = 0f;

        m[8] = 0f;
        m[9] = 0f;
        m[10] = -((f + n) / (f - n));
        m[11] = -1f;

        m[12] = 0f;
        m[13] = 0f;
        m[14] = -((2f * f * n) / (f - n));
        m[15] = 0f;

    }
}

