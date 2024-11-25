#include <iostream>
#include <sys/types.h>  // open()
#include <sys/stat.h>   // open()
#include <fcntl.h>      // open()
#include <unistd.h>     // close() read() write() fork() exec()
#include <errno.h>      // errno
#include <features.h>

#include "../src/log/log.h"
#include "../src/buffer/buffer.h"
#include "../src/http/http_conn.h"
#include "../src/pool/thread_pool.h"
#include "../src/pool/sqlconn_pool.h"
#include "../src/timer/heap_timer.h"


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
    // int open(const char *pathname, int flags, mode_t mode);

    // pathname：是一个指向以空字符结尾的字符串的指针，用于指定要打开的文件的路径和名称
    // flags：是一个整型参数，通过使用不同的宏（这些宏通常在 <fcntl.h> 头文件中定义）进行按位或（|）操作组合，来指定文件的打开方式
    // mode：这个参数用于指定文件创建时的初始权限

    // O_CREAT：表示如果指定的文件不存在，那么就创建这个文件
    // O_RDWR：指定以可读可写的模式打开文件
    // O_TRUNC：当要打开的文件已经存在时，使用这个宏会将文件的长度截断为 0，也就是会清空文件现有的所有内容，使其变为一个空文件
    // S_IRUSR：表示赋予文件所有者（用户）可读权限
    // S_IWUSR：赋予文件所有者（用户）可写权限

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
    lseek(test_fd, 0, SEEK_SET);  // 将文件指针移回文件开头，准备读取 lseek(test_fd, 0, )
    // off_t lseek(int fd, off_t offset, int whence);

    // fd：即文件描述符
    // offset：这是一个 off_t 类型的参数，它指定了要相对于 whence 参数所指示的起始位置移动的字节数
    // whence：这个整型参数用于确定偏移量的相对起始位置(SEEK_SET：文件开头； SEEK_CUR：当前文件的读写位置； SEEK_END: 文件末尾)

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

void testBuffer() {
    testBufferReadFromFd();
    testBufferWriteToFd();
}

// 测试Log类
void testLogger() {
    int cnt = 0, level = 0;
    Log::getInstance()->init(level, "./TestLogSync", ".log", false); // 同步写
    for(level = 3; level >= 0; level--) {
        Log::getInstance()->setLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }
    cnt = 0;
    Log::getInstance()->init(level, "./TestLogAsync", ".log", true); // 异步写
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
    for(int j = 0; j < 100; j++ ){
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

void testThreadPool() {
    Log::getInstance()->init(0, "./TestThreadPool", ".log", true); // 异步写
    threadPool threadpool(8);
    for(int i = 0; i < 24; i++) {
        threadpool.addTask(std::bind(threadLogTask, i % 4, i * 100));
    }
    getchar(); // 这里如果不获取字符输入的话线程池就会报错
}

// 测试sql连接池类
void testSqlPool() {
    /* logger和sql连接池初始化*/ 
    Log::getInstance()->init(0, "./TestSQLPool", ".log", true); // 异步写
    sqlConnPool::getInstance()->init("localhost", 3306, "root", "qq105311", "testdb", 16);

    // 初始化后sql连接池中空闲sql连接数目
    LOG_DEBUG("sqlconnpool free connection nums after init: %d", sqlConnPool::getInstance()->getFreeConnCount());
    MYSQL* conn = sqlConnPool::getInstance()->getConn();
    LOG_DEBUG("sqlconnpool free connection nums after getConn(): %d", sqlConnPool::getInstance()->getFreeConnCount());
    sqlConnPool::getInstance()->freeConn(conn);
    LOG_DEBUG("sqlconnpool free connection nums after freeConn(): %d", sqlConnPool::getInstance()->getFreeConnCount());

}

void testPools() {
    testSqlPool();
    testThreadPool();
}

int main() {
    testBuffer();
    printf("Test Buffer module end!\n");
    testLogger();
    printf("Test Logger module end!\n");
    testPools();
    printf("Test Pool module end!\n");
    // testHttp();
    // printf("Test Http module end!\n");
    // testTimer();
    // printf("Test Timer module end!\n");
    return 0;
}
