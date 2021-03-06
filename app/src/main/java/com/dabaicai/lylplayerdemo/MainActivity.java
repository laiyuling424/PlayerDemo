package com.dabaicai.lylplayerdemo;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;

import com.dabaicai.video.GLPlayerActivity;
import com.dabaicai.video.MainActivity2;
import com.dabaicai.video.PlayerActivity;


public class MainActivity extends AppCompatActivity {

    Button button;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initPermiss();

        button = findViewById(R.id.video);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                startActivity(new Intent(MainActivity.this, MainActivity2.class));
            }
        });
    }

    private void initPermiss() {
//        PermissionX.init(this)
//                .permissions(Manifest.permission.CAMERA, Manifest.permission.READ_EXTERNAL_STORAGE,
//                        Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.RECORD_AUDIO)
//                .onExplainRequestReason(new ExplainReasonCallback() {
//                    @Override
//                    public void onExplainReason(ExplainScope scope, List<String> deniedList) {
//                        scope.showRequestReasonDialog(deniedList, "应用运行需要一些核心权限,人脸验证需要相机,麦克风和储存权限等", "确定", "取消");
//                    }
//                })
//                .onForwardToSettings(new ForwardToSettingsCallback() {
//                    @Override
//                    public void onForwardToSettings(ForwardScope scope, List<String> deniedList) {
//                        scope.showForwardToSettingsDialog(deniedList, "应用需要一些必要权限,需要去权限管理界面去设置", "确定", "取消");
//                    }
//                })
//                .request(new RequestCallback() {
//                    @Override
//                    public void onResult(boolean allGranted, List<String> grantedList, List<String> deniedList) {
//
//                    }
//                });

    }

    public void player(View view) {
        startActivity(new Intent(MainActivity.this, PlayerActivity.class));
    }

    public void opengl(View view) {
        startActivity(new Intent(MainActivity.this, GLPlayerActivity.class));
    }
}