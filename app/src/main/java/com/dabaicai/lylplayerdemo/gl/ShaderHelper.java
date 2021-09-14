package com.dabaicai.lylplayerdemo.gl;

import android.opengl.GLES20;
import android.util.Log;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/13 7:11 下午
 */
public class ShaderHelper {

    public static int compileShader(int type, String source) {
        //创建shader
        int shaderId = GLES20.glCreateShader(type);
        if (shaderId == 0) {
            Log.d("mmm", "创建shader失败");
            return 0;
        }
        //上传shader源码
        GLES20.glShaderSource(shaderId, source);
        //编译shader源代码
        GLES20.glCompileShader(shaderId);
        //取出编译结果
        int[] compileStatus = new int[1];
        //取出shaderId的编译状态并把他写入compileStatus的0索引
        GLES20.glGetShaderiv(shaderId, GLES20.GL_COMPILE_STATUS, compileStatus, 0);
        Log.d("mmm编译状态", GLES20.glGetShaderInfoLog(shaderId));

        if (compileStatus[0] == 0) {
            GLES20.glDeleteShader(shaderId);
            Log.d("mmm", "创建shader失败");
            return 0;
        }

        return shaderId;
    }


    public static int linkProgram(int mVertexshader, int mFragmentshader) {
        //创建程序对象
        int programId = GLES20.glCreateProgram();
        if (programId == 0) {
            Log.d("mmm", "创建program失败");
            return 0;
        }
        //依附着色器
        GLES20.glAttachShader(programId, mVertexshader);
        GLES20.glAttachShader(programId, mFragmentshader);
        //链接程序
        GLES20.glLinkProgram(programId);
        //检查链接状态
        int[] linkStatus = new int[1];
        GLES20.glGetProgramiv(programId, GLES20.GL_LINK_STATUS, linkStatus, 0);
        Log.d("mmm", "链接程序" + GLES20.glGetProgramInfoLog(programId));
        if (linkStatus[0] == 0) {
            GLES20.glDeleteProgram(programId);
            Log.d("mmm", "链接program失败");
            return 0;
        }

        return programId;

    }

    public static boolean volidateProgram(int program) {
        GLES20.glValidateProgram(program);
        int[] validateStatus = new int[1];
        GLES20.glGetProgramiv(program, GLES20.GL_VALIDATE_STATUS, validateStatus, 0);
        Log.d("mmm", "当前openl情况" + validateStatus[0] + "/" + GLES20.glGetProgramInfoLog(program));

        return validateStatus[0] != 0;
    }


    public static int buildProgram(String vertex_shader_source, String fragment_shader_source) {
        //编译着色器源码
        int mVertexshader = compileShader(GLES20.GL_VERTEX_SHADER, vertex_shader_source);
        int mFragmentshader = compileShader(GLES20.GL_FRAGMENT_SHADER, fragment_shader_source);
        //链接程序
        int program = ShaderHelper.linkProgram(mVertexshader, mFragmentshader);
        //验证opengl对象
        ShaderHelper.volidateProgram(program);

        return program;
    }
}


