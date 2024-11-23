#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <cassert>

class threadPool {
public:
    threadPool() = default;
    threadPool(threadPool&&) = default;
    // 尽量用make_shared代替new，如果通过new再传递给shared_ptr，内存是不连续的，会造成内存碎片化
    explicit threadPool(int threadCount = 8) : pool_(std::make_shared<pool>()) { // make_shared:传递右值，功能是在动态内存中分配一个对象并初始化它，返回指向此对象的shared_ptr
        assert(threadCount > 0);
        for(int i = 0; i < threadCount; i++) {
            // 创建 thread，让它去执行声明的匿名函数，且创建完毕就 detach()，由操作系统调度
            std::thread([this]() { // this 捕获当前类对象
                std::unique_lock<std::mutex> locker(pool_->mtx_);
                while(true) {
                    if(!pool_->tasks_.empty()) {
                        auto task = std::move(pool_->tasks_.front());
                        pool_->tasks_.pop();
                        locker.unlock(); // 任务获取完毕，解锁任务队列，让其他线程可以去获取任务
                        task();
                        locker.lock(); // 马上又要取任务了，上锁
                    } else if(pool_->isClosed_) {
                        break;
                    } else {
                        pool_->cond_.wait(locker); // // 如果任务队列空，并且线程池未关闭，就等待addTask的的通知
                    }

                }

            }).detach();
        }
    }

    ~threadPool() {
        if(pool_) {
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            pool_->isClosed_ = true;
        }
        pool_->cond_.notify_all(); // 唤醒所有线程
    }

    template<typename T>
    void addTask(T&& task) {
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->tasks_.emplace(std::forward<T>(task));
        pool_->cond_.notify_one();
    }

private:
    struct pool {
        std::mutex mtx_;
        std::condition_variable cond_;
        bool isClosed_;
        std::queue<std::function<void()>> tasks_; // 任务队列，函数类型为void()
    };
    std::shared_ptr<pool> pool_;
};
#endif