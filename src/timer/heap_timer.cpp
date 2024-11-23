#include "heap_timer.h"

void heapTimer::add(int id, int timeOut, const timeOutCallBack& cb) {
    assert(id >= 0);
    
    if(ref_.count(id)) { // 如果有，则调整 expires
        int tmp = ref_[id];
        heap_[tmp].expires = clock_::now() + ms(timeOut);
        heap_[tmp].cb = cb;
        if(!siftdown_(tmp, heap_.size())) {
            siftup_(tmp);
        }
    } else { // 没有则新增节点
        size_t n = heap_.size();
        ref_[id] = n;
        heap_.push_back({id, clock_::now() + ms(timeOut), cb}); // 结构体默认构造函数 timerNode node = {id, clock_::now() + ms(timeOut), cb}
        siftup_(n);
    }
}

// 删除超时节点
void heapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        timerNode node = heap_.front();
        if(std::chrono::duration_cast<ms>(node.expires - clock_::now()).count() > 0) { // 当前节点未超时，不再需要删除节点跳出循环
            break;
        }
        node.cb();
        pop();
    }
}

// 删除指定id,并触发回调函数
void heapTimer::removeTarget(int id) {
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    auto node = heap_[i];
    node.cb(); // 触发回调函数
    del_(i);
}

// 调整指定id的节点
void heapTimer::adjust(int id, int newExpires) {
    assert(!heap_.empty() && ref_.count(id));
    heap_[ref_[id]].expires = clock_::now() + ms(newExpires);
    siftdown_(ref_[id], heap_.size());
}

void heapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

void heapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

int heapTimer::getNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<ms>(heap_.front().expires - clock_::now()).count();
        if(res < 0) { res = 0;}
    }
    return res;
}

void heapTimer::del_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    // 将删除的节点换到队尾，然后调整堆
    size_t tmp = i;
    size_t n = heap_.size() - 1;
    assert(tmp <= n);
    
    if(tmp < heap_.size() - 1) { // 如果在队尾就不用动了
        swapNode_(tmp, heap_.size() - 1);
        if(!siftdown_(tmp, n)) {
            siftup_(tmp);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void heapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t parent = (i - 1) / 2;
    while(parent >= 0) {
        if(heap_[parent] > heap_[i]) {
            swapNode_(i, parent);
            i = parent;
            parent = (i - 1) / 2;
        } else {
            break;
        }
    }
}

// false: 不需要下滑；true: 下滑成功
bool heapTimer::siftdown_(size_t i, size_t n) {
    assert(i >= 0 && i < heap_.size());
    assert(n >= 0 && n <= heap_.size()); // n：共几个节点
    auto idx = i;
    auto child = idx * 2 + 1;
    while(child < n) {
        if(child + 1 < n && heap_[child + 1] < heap_[child]) {
            child++;
        }
        if(heap_[child] < heap_[idx]) {
            swapNode_(idx, child);
            idx = child;
            child = idx * 2 + 1;
        } else {
            break;
        }
    }
    return idx > i; // idx 大于初始 i,则说明成功进行的下移
}

void heapTimer::swapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    swap(heap_[i], heap_[j]);
    // 交换节点后，节点内部成员 id 也需要调整
    ref_[heap_[i].id] = i; 
    ref_[heap_[j].id] = j;
}