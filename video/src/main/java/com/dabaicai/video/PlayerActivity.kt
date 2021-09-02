package com.dabaicai.video

import android.os.Bundle
import android.view.SurfaceView
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.dabaicai.video.ffmpeg.PlayerControl

class PlayerActivity : AppCompatActivity(), PlayerControl.PlayerControlCallBack {

    private lateinit var surfaceView: SurfaceView
    private lateinit var fps: TextView
    private lateinit var time: TextView
    private lateinit var errorInfo: TextView
    private lateinit var seekBar: SeekBar
    private lateinit var statusButton: Button
    private lateinit var allButton: Button
    private lateinit var nextButton: Button
    private lateinit var frontButton: Button
    private lateinit var fastButton: Button
    private lateinit var slowButton: Button
    private lateinit var videoFastButton: Button
    private lateinit var audioFastButton: Button
    private lateinit var p1Button: Button
    private lateinit var p2Button: Button


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_player)
        initView()
    }

    private fun initView() {
        surfaceView = findViewById(R.id.surfaceView)
        fps = findViewById(R.id.fps)
        time = findViewById(R.id.time)
        errorInfo = findViewById(R.id.error)
        seekBar = findViewById(R.id.seekBar)
        statusButton = findViewById(R.id.statusButton)
        allButton = findViewById(R.id.allButton)
        nextButton = findViewById(R.id.nextButton)
        frontButton = findViewById(R.id.frontButton)
        fastButton = findViewById(R.id.fastButton)
        slowButton = findViewById(R.id.slowButton)
        videoFastButton = findViewById(R.id.videoFastButton)
        audioFastButton = findViewById(R.id.audioFastButton)
        p1Button = findViewById(R.id.p1Button)
        p2Button = findViewById(R.id.p2Button)

        statusButton.setOnClickListener {

        }

    }

    override fun error(code: Int, message: String) {
        errorInfo.text = message
    }

    override fun ready(alltime: Int) {

    }

    override fun status(status: PlayerControl.PlayerStatus) {
        if (status == PlayerControl.PlayerStatus.NONE) {

        } else if (status == PlayerControl.PlayerStatus.PREPARE) {

        } else if (status == PlayerControl.PlayerStatus.PLAYING) {
            statusButton.text = "stop"
        } else if (status == PlayerControl.PlayerStatus.STOP) {
            statusButton.text = "play"
        } else if (status == PlayerControl.PlayerStatus.DESTORY) {

        } else if (status == PlayerControl.PlayerStatus.ACTIONDO) {

        }
    }

    override fun videoInfo(fps: Int, time: Int, allTime: Int) {
        this.fps.text = "fps $fps"
        this.time.text = "$time/$allTime"
    }
}