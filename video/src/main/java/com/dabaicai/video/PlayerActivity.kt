package com.dabaicai.video

import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.SurfaceView
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.dabaicai.video.ffmpeg.PlayerControl
import java.io.File

class PlayerActivity : AppCompatActivity(), PlayerControl.PlayerControlCallBack, SeekBar.OnSeekBarChangeListener {

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


    private lateinit var playerControl: PlayerControl


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

        seekBar.setOnSeekBarChangeListener(this)
        seekBar.isEnabled = false

        statusButton.setOnClickListener {
            playerControl.startOrStop()
        }

        playerControl = PlayerControl(surfaceView)
        playerControl.setCallBack(this)
        //            File file = new File("/storage/emulated/0/aa.mp4");
        val file = File(Environment.getExternalStorageDirectory(), "aa.mp4")
        if (file.exists()) {
            playerControl.setPath(file.absolutePath)
        }

    }

    override fun error(code: Int, message: String) {
        errorInfo.post {
            errorInfo.text = message
        }
    }

    override fun ready(alltime: Int) {
//        statusButton.text = "pause"
        statusButton.post {
            statusButton.text = "pause"
            seekBar.max = alltime
            seekBar.isEnabled = true
        }
    }

    override fun status(status: PlayerControl.PlayerStatus) {
        statusButton.post {
            if (status == PlayerControl.PlayerStatus.NONE) {

            } else if (status == PlayerControl.PlayerStatus.PREPARE) {

            } else if (status == PlayerControl.PlayerStatus.PLAYING) {
                statusButton.text = "pause"
            } else if (status == PlayerControl.PlayerStatus.STOP) {
                statusButton.text = "play"
            } else if (status == PlayerControl.PlayerStatus.DESTORY) {

            } else if (status == PlayerControl.PlayerStatus.ACTIONDO) {

            } else if (status == PlayerControl.PlayerStatus.PAUSE) {
                statusButton.text = "resume"
            }
        }
    }

    override fun videoInfo(fps: Int, time: Int, allTime: Int) {
        seekBar.post {
            this.fps.text = "fps $fps"
            this.time.text = "$time/$allTime"
            seekBar.progress = time
        }
    }

    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        Log.d("lyll", "progress is $progress,fromUser is $fromUser")
    }

    override fun onStartTrackingTouch(seekBar: SeekBar?) {
        Log.d("lyll", "onStartTrackingTouch is ${seekBar!!.progress}")
    }

    override fun onStopTrackingTouch(seekBar: SeekBar?) {
        Log.d("lyll", "onStopTrackingTouch is ${seekBar!!.progress}")
    }
}