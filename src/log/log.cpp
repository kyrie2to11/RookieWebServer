#include "log.h"

// 构造函数
Log::Log() {
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
    lineCount_ = 0;
    today_ = 0;
    isAsync_ = false;
}

Log::~Log() {
    while(!deque_->empty()) {
        deque_->flush(); // 唤醒消费者，处理掉剩下的任务
    }
    deque_->close(); // 关闭队列
    writeThread_->join(); // 等待当前线程完成手中任务
    if(fp_) { // 冲洗文件缓冲区，关闭文件描述符
        lock_guard<mutex> locker(mtx_);
        flush(); // 清空缓冲区数据
        fclose(fp_); // 关闭日志文件
    }
}

// 唤醒阻塞队列消费者，开始写日志
void Log::flush() {
    if(isAsync_) { // 只有异步日志才会用到deque
        deque_->flush();
    }
    fflush(fp_); // 强制将缓冲区中的内容写入文件fp_(调用fputs(str.c_str(), fp_)后str会先被放入内存的缓冲区中，而后根据预设策略如：全缓冲/行缓冲/无缓冲 写入磁盘)
}

// 懒汉模式 局部静态变量法 （这种方法不需要加锁解锁操作）
Log* Log::getInstance() {
    static Log log;
    return &log;
}

// 异步日志的写线程函数
void Log::flushLogThread() {
    Log::getInstance()->asyncWrite_();
}

// 写线程的真正执行函数
void Log::asyncWrite_() {
    string str = "";
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

// 初始化日志实例
void Log::init(int level, const char* path, const char* suffix, bool isAsync) {
    isOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    if(isAsync) { // 异步方式
        isAsync_ = true;
        if(!deque_) { // 为空则创建一个
            unique_ptr<blockQueue<std::string>> newQue(new blockQueue<std::string>);
            // unique_ptr不支持普通的拷贝或赋值操作，所以用move
            // 将动态申请的内存权给deque, newDeque被释放
            deque_ = move(newQue); // 左值变右值，掏空newDeque

            unique_ptr<thread> newThread(new thread(flushLogThread));
            writeThread_ = move(newThread);
        }
    } else { // 同步方式
            isAsync_ = false;
    }

    lineCount_ = 0;
    time_t timer = time(nullptr);
    struct tm* systime = localtime(&timer);
    char fileName[LOG_NAME_LEN] = {0}; // 将数组中的所有元素初始化为字符 '\0'
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
            path_, systime->tm_year + 1900, systime->tm_mon + 1, systime->tm_mday, suffix_);
    today_ = systime->tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.retrieveAll();
        if(fp_) { // 关闭fp_，然后重新重新打开
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a"); // 打开文件读取并附加写入
        if(fp_ == nullptr) {
            mkdir(path_, 0777); // 生成目录文件（最大权限）
            fp_ = fopen(fileName, "a"); 
        }
        assert(fp_ != nullptr);
    }

}

void Log::write(int level, const char* format, ...) {
    // 获取当前时间，保存在struct tm t中
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm* sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList; // va_list （字符指针），这个变量会被后续的宏（va_start、va_end 等）用来指向可变参数列表中的参数，从而实现对可变参数的访问操作。

    // 日志日期 日志行数 如果不是今天或行数超了
    if(today_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0))){
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if(today_ != t.tm_mday) { // 时间不匹配， 则替换为最新的日志文件名
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            today_ = t.tm_mday;
            lineCount_ = 0;
        } else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }

        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    // 在buffer内生成一条对应的日志新信息
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        
        buff_.hasWritten(n);
        appendLogLevelTitle_(level);

        va_start(vaList, format); // va_start：对vaList进行初始化，让vaList指向可变参数表里面的第一个参数。va_start()第一个参数是vaList本身，第二个参数是在变参表前面紧挨着的一个变量，即“...”之前的那个参数；
        int m = vsnprintf(buff_.beginWrite(), buff_.writableBytes(), format, vaList);
        va_end(vaList); // va_end: 释放指针，将输入的参数 vaList 置为 NULL。通常va_start和va_end是成对出现。

        buff_.hasWritten(m);
        buff_.append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) { // 异步方式（加入阻塞队列中，等待写线程读取日志信息）
            deque_->push_back(buff_.retrieveAllAsString());
        } else { // 同步方式
            fputs(buff_.peek(), fp_); // 同步就直接写入文件，从 buff_ 可读位置开始直到遇到结束标志"\0"
        }
        buff_.retrieveAll(); // 清空buff
    }
}

// 添加日志等级
void Log::appendLogLevelTitle_(int level) {
    switch(level) {
        case 0:
            buff_.append("[debug]: ", 9);
            break;
        case 1:
            buff_.append("[info] : ", 9);
            break;
        case 2:
            buff_.append("[warn] : ", 9);
            break;
        case 3:
            buff_.append("[error]: ", 9);
            break;
        default:
            buff_.append("[info] : ", 9);
            break;
    }
}

int Log::getLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::setLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}