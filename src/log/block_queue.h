#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <assert.h>
#include <sys/time.h>
using namespace std;

// 模板类的声明和定义一般同时放在头文件中，以解决"模板的分离编译问题"
template<typename T>
class blockQueue {
private:
    deque<T> deq_;
    mutex mtx_;
    bool isClose_;
    size_t capacity_;
    condition_variable condConsumer_;
    condition_variable condProducer_;

public:
    explicit blockQueue(size_t maxSize = 1000);
    ~blockQueue();
    void close(); 
    void clear();
    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T& item); // 弹出任务放入item
    bool pop(T& item, int timeout); // 等待时间
    
    T front();
    T back();
    size_t capacity();
    size_t size();
    
    void flush();
    
};

template<typename T>
blockQueue<T>::blockQueue(size_t maxSize) : capacity_(maxSize) {
    assert(maxSize > 0);
    isClose_ = false;
}

template<typename T>
blockQueue<T>::~blockQueue() {
    close();
}

template<typename T>
void blockQueue<T>::close() {
    clear();
    isClose_ = true;
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template<typename T>
void blockQueue<T>::clear() {
    lock_guard<mutex> locker(mtx_);
    deq_.clear();
}

template<typename T>
bool blockQueue<T>::empty() {
    lock_guard<mutex> locker(mtx_);
    return deq_.empty();
}

template<typename T>
bool blockQueue<T>::full() {
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

// 生产者
template<typename T>
void blockQueue<T>::push_back(const T& item) {
    unique_lock<mutex> locker(mtx_);                  // 获取锁
    while(deq_.size() >= capacity_) {                 // 队列满了
        condProducer_.wait(locker);                   // 生产者暂停生产，释放锁，等待消费者唤醒生产条件变量condProducer_，再重新获取锁
    }
    deq_.push_back(item);
    condConsumer_.notify_one();                       // 生产者唤醒消费条件变量 condConsumer_
}

// 生产者
template<typename T>
void blockQueue<T>::push_front(const T& item) {
    unique_lock<mutex> locker(mtx_);                  // 获取锁
    while(deq_.size() >= capacity_) {                 // 队列满了
        condProducer_.wait(locker);                   // 生产者暂停生产，释放锁，等待消费者唤醒生产条件变量condProducer_，再重新获取锁
    }
    deq_.push_front(item);
    condConsumer_.notify_one();                       // 生产者唤醒消费条件变量 condConsumer_
}

// 消费者
template<typename T>
bool blockQueue<T>::pop(T& item) {
    unique_lock<mutex> locker(mtx_); // 获取锁
    while(deq_.empty()) {
        condConsumer_.wait(locker);  // 队列空，消费者等待，释放锁，等待生产者唤醒消费条件变量condConsumer_，再重新获取锁
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();      // 消费者唤醒生产条件变量 condProducer_
    return true;
}

// 消费者
template<typename T>
bool blockQueue<T>::pop(T& item, int timeout) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty()) {
        // 让当前线程释放锁并进入等待状态，等待条件变量 condConsumer_ 对应的条件变为真，并且设置了一个最长等待时间为 timeout 秒
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) { // 如果等待的结果是超时
            return false;
        }
        if(isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one(); // 唤醒生产者
    return true;
}

template<typename T>
T blockQueue<T>::front() {
    lock_guard<mutex> locker(mtx_);
    return deq_.front();
}

template<typename T>
T blockQueue<T>::back() {
    lock_guard<mutex> locker(mtx_);
    return deq_.back();
}

template<typename T>
size_t blockQueue<T>::capacity() {
    lock_guard<mutex> locker(mtx_);
    return capacity_;
}

template<typename T>
size_t blockQueue<T>::size() {
    lock_guard<mutex> locker(mtx_);
    return deq_.size();
}

// 唤醒消费者
template<typename T>
void blockQueue<T>::flush() {
    condConsumer_.notify_one();
}
#endif