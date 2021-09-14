package com.dabaicai.lylplayerdemo.gl;

import android.content.Context;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/13 7:11 下午
 */
public class ReadResouceText {

    public static String readResoucetText(Context context, int resouceId) {
        StringBuffer body = new StringBuffer();

        try {
            InputStream inputStream = context.getResources().openRawResource(resouceId);
            InputStreamReader inputStreamReader = new InputStreamReader(inputStream);
            BufferedReader bufferedReader = new BufferedReader(inputStreamReader);
            String nextline;
            while ((nextline = bufferedReader.readLine()) != null) {
                body.append(nextline);
                body.append("\n");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }

        return body.toString();
    }
}

