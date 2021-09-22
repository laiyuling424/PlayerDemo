//
// Created by 赖於领 on 2021/9/3.
//

/**
 * author : lyl
 * e-mail : laiyuling424@gmail.com
 * date   : 2021/9/3 11:13 上午
 */
#ifndef LYLPLAYERDEMO_LOCK_QUEUE_H
#define LYLPLAYERDEMO_LOCK_QUEUE_H

#include <queue>
#include <pthread.h>

using namespace std;

template<typename T>
class LockQueue {
private:
    typedef void (*ReleaseData)(T &);

    //互斥锁
    pthread_mutex_t _mutex;
    //条件变量
    pthread_cond_t _cond;
    //队列
    queue<T> _queue;
    //释放数据
    ReleaseData releaseData;
    //是否在工作
    int work;
public:
    LockQueue() {
        pthread_mutex_init(&_mutex, NULL);
        pthread_cond_init(&_cond, NULL);
        work = 0;
    }

    ~LockQueue() {
        clear();
        pthread_cond_destroy(&_cond);
        pthread_mutex_destroy(&_mutex);
    }

    void setWork(bool isWork) {
        pthread_mutex_lock(&_mutex);
        this->work = isWork;
        pthread_cond_signal(&_cond);
        pthread_mutex_unlock(&_mutex);

    }

    void setReleaseData(ReleaseData r) {
        releaseData = r;
    }

    /**
     * 将 queue 里面的数据清除
     * 有可能持有外面的引用
     */
    void clear() {
        pthread_mutex_lock(&_mutex);
        int size = _queue.size();
        for (int i = 0; i < size; ++i) {
            T data = _queue.front();
            releaseData(data);
            _queue.pop();
        }
        pthread_mutex_unlock(&_mutex);
    }

    void destory() {
        clear();
        work = 0;
        pthread_cond_destroy(&_cond);
        pthread_mutex_destroy(&_mutex);
    }

    void push(T data) {
        pthread_mutex_lock(&_mutex);
        if (!work) {
            return;
        }
        bool isSingle = empty();
        _queue.push(data);
        if (isSingle) {
            pthread_cond_signal(&_cond);
        }
        pthread_mutex_unlock(&_mutex);
    }

    int pop(T &data) {
        pthread_mutex_lock(&_mutex);
        if (!work) {
            return 0;
        }
        int num = 0;
        if (empty()) {
            pthread_cond_wait(&_cond, &_mutex);
        }
        data = _queue.front();
        _queue.pop();
        num = 1;
        pthread_mutex_unlock(&_mutex);
        return num;
    }

    bool empty() {
        return _queue.empty();
    }

    int size() {
        return _queue.size();
    }
};

#endif //LYLPLAYERDEMO_LOCK_QUEUE_H
