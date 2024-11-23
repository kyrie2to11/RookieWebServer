#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <features.h>

#include "../src/log/log.h"
#include "../src/buffer/buffer.h"
#include "../src/http/http_request.h"
#include "../src/pool/thread_pool.h"


#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

// 测试Buffer类的readFd函数功能
void testBufferReadFromFd() {
    Buffer buffer(1024, 8);
    int errno_value;
    // 创建一个测试文件并写入一些测试数据
    int test_fd = open("test.txt", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if (test_fd == -1) {
        std::cerr << "创建测试文件失败" << std::endl;
        return;
    }
    const char* test_data = "Hello, World!";
    ssize_t write_result = write(test_fd, test_data, strlen(test_data));
    if (write_result == -1) {
        std::cerr << "写入测试文件失败" << std::endl;
        close(test_fd);
        return;
    }
    lseek(test_fd, 0, SEEK_SET);  // 将文件指针移回文件开头，准备读取

    // 调用Buffer的readFd函数读取文件内容到缓冲区
    ssize_t read_result = buffer.readFd(test_fd, &errno_value);
    if (read_result == -1) {
        std::cerr << "读取文件到缓冲区失败，错误码: " << errno_value << std::endl;
    } else {
        std::cout << "成功从文件读取 " << read_result << " 字节数据到缓冲区" << std::endl;
        // 可以进一步验证缓冲区中的数据是否正确，比如通过查看可读字节数以及部分数据内容等方式
        std::cout << "缓冲区可读字节数: " << buffer.readableBytes() << std::endl;
        std::string read_data = buffer.retrieveAsString(buffer.readableBytes());
        std::cout << "读取到的数据: " << read_data << std::endl;
    }
    remove("test.txt");
    close(test_fd);

}

// 测试Buffer类的writeFd函数功能
void testBufferWriteToFd() {
    Buffer buffer(1024, 8);
    const char* test_data = "This is a test string.";
    buffer.append(test_data, strlen(test_data));
    int errno_value;
    int test_fd = open("output.txt", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if (test_fd == -1) {
        std::cerr << "创建输出文件失败" << std::endl;
        return;
    }

    // 调用Buffer的writeFd函数将缓冲区内容写入文件
    ssize_t write_result = buffer.writeFd(test_fd, &errno_value);
    if (write_result == -1) {
        std::cerr << "将缓冲区内容写入文件失败，错误码: " << errno_value << std::endl;
    } else {
        std::cout << "成功将 " << write_result << " 字节数据从缓冲区写入文件" << std::endl;
    }
    remove("output.txt");
    close(test_fd);
}

// 测试Log类
void testLog() {
    int cnt = 0, level = 0;
    Log::getInstance()->init(level, "./testlog1", ".log", false); // 同步写
    for(level = 3; level >= 0; level--) {
        Log::getInstance()->setLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }
    cnt = 0;
    Log::getInstance()->init(level, "./testlog2", ".log", true); // 异步写
    for(level = 0; level < 4; level++) {
        Log::getInstance()->setLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 222222222 %d ============= ", "Test", cnt++);
                
            }
        }
    }
}

// 测试线程池类
void threadLogTask(int i, int cnt) {
    for(int j = 0; j < 10000; j++ ){
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

void testThreadPool() {
    Log::getInstance()->init(0, "./testThreadpool", ".log", true); // 异步写
    threadPool threadpool(6);
    for(int i = 0; i < 24; i++) {
        threadpool.addTask(std::bind(threadLogTask, i % 4, i * 10000));
    }
    getchar();
}

int main() {
    testBufferReadFromFd();
    testBufferWriteToFd();
    printf("Test Buffer end!\n");
    testLog();
    printf("Test Logger end!\n");
    testThreadPool();
    printf("Test Threadpool end!\n");
    // testHttpRequest();
    // printf("Test Http request end!\n");
    return 0;
}
