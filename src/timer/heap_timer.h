#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> timeOutCallBack;
typedef std::chrono::high_resolution_clock clock_;
typedef std::chrono::milliseconds ms;
typedef clock_::time_point timeStamp;

struct timerNode {
    int id;
    timeStamp expires; // 超时时间点
    timeOutCallBack cb; // 回调函数 function<void()>
    bool operator<(const timerNode& t) { // 重载"<"
        return expires < t.expires;
    }
    bool operator>(const timerNode& t) { // 重载">"
        return expires > t.expires;
    }
};

class heapTimer {
public: 
    heapTimer() { heap_.reserve(64); } // 保留（扩充）容量
    ~heapTimer() { clear(); }

    void add(int id, int timeOut, const timeOutCallBack& cb); // 增加节点
    void tick(); // 删除头部
    void removeTarget(int id); // 删除指定节点
    void adjust(int id, int newExpires); // 改变指定id的expires后调整堆
    void clear();
    void pop();
    int getNextTick();

private:
    void del_(size_t i);
    void siftup_(size_t i);
    bool siftdown_(size_t i, size_t n);
    void swapNode_(size_t i, size_t j);

    std::vector<timerNode> heap_;
    // key: id value:vector heap_索引
    std::unordered_map<int, size_t> ref_;
};

#endif