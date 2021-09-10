# PlayerDemo

2021
9.2         写 java 层
9.4         视频播放
9.7         音视频同步完成
9.9         c++ 回调 java，这个可太难了
9.10        pause 之后 resume 产生的 anr 问题（没有明白 看 AudioChannel TODO）
            seek
            视频/音频加速（没测）

注意点：
    1.在 c++ 子线程调用 java 代码，在 java 里变成了子线程 ，这个太有问题了
        刚开始是自己判断线程是对的，后来线程就莫名其妙了，太奇怪了
        看了其他 dranne 的就是自己传的参数 thread 来判断是不是主线程的

    2.
