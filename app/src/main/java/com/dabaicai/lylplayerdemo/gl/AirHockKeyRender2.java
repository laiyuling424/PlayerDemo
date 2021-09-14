package com.dabaicai.lylplayerdemo.gl;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;

import com.dabaicai.lylplayerdemo.R;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/14 11:27 上午
 */
public class AirHockKeyRender2 implements GLSurfaceView.Renderer {

    //调整宽高比
    private final FloatBuffer verticeData;
    private final int BYTES_PER_FLOAT = 4;
    private final int COLOR_COMPONENT_COUNT = 3;
    private final Context mContext;
    //逆时针绘制三角形
    float[] tableVertices = {
            //顶点
            0f, 0f,
            //顶点颜色值
            1f, 1f, 1f,

            -0.5f, -0.8f,
            0.7f, 0.7f, 0.7f,

            0.5f, -0.8f,
            0.7f, 0.7f, 0.7f,

            0.5f, 0.8f,
            0.7f, 0.7f, 0.7f,

            -0.5f, 0.8f,
            0.7f, 0.7f, 0.7f,

            -0.5f, -0.8f,
            0.7f, 0.7f, 0.7f,

            //线
            -0.5f, 0f,
            1f, 0f, 0f,

            0.5f, 0f,
            0f, 1f, 0f,

            //点
            0f, -0.4f,
            1f, 0f, 0f,

            0f, 0.4f,
            0f, 0f, 1f
    };
    private int POSITION_COMPONENT_COUNT = 2;
    private final int STRIDE = (POSITION_COMPONENT_COUNT + COLOR_COMPONENT_COUNT) * BYTES_PER_FLOAT;
    private int a_position;
    private int a_color;

    private float[] mProjectionMatrix = new float[16];
    private int u_matrix;


    public AirHockKeyRender2(Context context) {

        this.mContext = context;
        //把float加载到本地内存
        verticeData = ByteBuffer.allocateDirect(tableVertices.length * BYTES_PER_FLOAT)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer()
                .put(tableVertices);
        verticeData.position(0);
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        //当surface被创建时，GlsurfaceView会调用这个方法，这个发生在应用程序
        // 第一次运行的时候或者从其他Activity回来的时候也会调用

        //清空屏幕
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        //读取着色器源码
        String fragment_shader_source = ReadResouceText.readResoucetText(mContext, R.raw.fragment_shader2);
        String vertex_shader_source = ReadResouceText.readResoucetText(mContext, R.raw.vertex_shader2);

        //编译着色器源码
        int mVertexshader = ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, vertex_shader_source);
        int mFragmentshader = ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, fragment_shader_source);
        //链接程序
        int program = ShaderHelper.linkProgram(mVertexshader, mFragmentshader);

        //验证opengl对象
        ShaderHelper.volidateProgram(program);
        //使用程序
        GLES20.glUseProgram(program);

        //获取shader属性
        a_position = GLES20.glGetAttribLocation(program, "a_Position");
        a_color = GLES20.glGetAttribLocation(program, "a_Color");
        u_matrix = GLES20.glGetUniformLocation(program, "u_Matrix");


        //绑定a_position和verticeData顶点位置
        /**
         * 第一个参数，这个就是shader属性
         * 第二个参数，每个顶点有多少分量，我们这个只有来个分量
         * 第三个参数，数据类型
         * 第四个参数，只有整形才有意义，忽略
         * 第5个参数，一个数组有多个属性才有意义，我们只有一个属性，传0
         * 第六个参数，opengl从哪里读取数据
         */
        verticeData.position(0);
        GLES20.glVertexAttribPointer(a_position, POSITION_COMPONENT_COUNT, GLES20.GL_FLOAT,
                false, STRIDE, verticeData);
        //开启顶点
        GLES20.glEnableVertexAttribArray(a_position);

        verticeData.position(POSITION_COMPONENT_COUNT);
        GLES20.glVertexAttribPointer(a_color, COLOR_COMPONENT_COUNT, GLES20.GL_FLOAT,
                false, STRIDE, verticeData);
        //开启顶点
        GLES20.glEnableVertexAttribArray(a_color);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        //在Surface创建以后，每次surface尺寸大小发生变化，这个方法会被调用到，比如横竖屏切换

        //设置屏幕的大小
        GLES20.glViewport(0, 0, width, height);

        float a = width > height ? (float) width / (float) height : (float) height / (float) width;

        if (width > height) {
            Matrix.orthoM(mProjectionMatrix, 0, -a, a, -1f, 1f, -1f, 1f);
        } else {
            Matrix.orthoM(mProjectionMatrix, 0, -1f, 1f, -a, a, -1f, 1f);
        }
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        //当绘制每一帧数据的时候，会调用这个放方法，这个方法一定要绘制一些东西，即使只是清空屏幕
        //因为这个方法返回后，渲染区的数据会被交换并显示在屏幕上，如果什么都没有话，会看到闪烁效果

        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

        GLES20.glUniformMatrix4fv(u_matrix, 1, false, mProjectionMatrix, 0);


        //绘制长方形
        //指定着色器u_color的颜色为白色
        /**
         * 第一个参数：绘制绘制三角形
         * 第二个参数：从顶点数组0索引开始读
         * 第三个参数：读入6个顶点
         *
         * 最终绘制俩个三角形，组成矩形
         */
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_FAN, 0, 6);

        //绘制分割线

        GLES20.glDrawArrays(GLES20.GL_LINES, 6, 2);

        //绘制点
        GLES20.glDrawArrays(GLES20.GL_POINTS, 8, 1);

        GLES20.glDrawArrays(GLES20.GL_POINTS, 9, 1);
    }
}

