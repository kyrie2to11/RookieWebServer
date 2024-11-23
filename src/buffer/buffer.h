#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <vector>
#include <assert.h>
#include <unistd.h> // write
#include <sys/uio.h> // readv
#include <errno.h> // errno

class Buffer {
private:
    std::vector<char> buffer_;  
    int readIndex_;  // 读的下标
    int writeIndex_; // 写的下标
    const size_t kCheapPrepend_; // 预留空间大小
    const size_t kInitBuffSize_; // buffer大小
    char* begin_();  // buffer开头指针
    const char* begin_() const;

    void makeSpace_(size_t len); // buffer扩容

public:
    Buffer(size_t initBuffSize = 1024, size_t cheapPrepend = 8);
    ~Buffer() = default;

    size_t writableBytes() const;  // 可在后方写的大小
    size_t readableBytes() const ; // 可读区域大小
    size_t prependableBytes() const; // 在前方预留空间的大小

    const char* peek() const; // 返回首个可读的元素指针
    void retrieve(size_t len); // 读取buffer后移动readIndex_
    void retrieveUntil(const char* end);
    void retrieveAll();
    std::string retrieveAsString(size_t len); // 读取指定长度的buffer返回string
    std::string retrieveAllAsString();

    void ensureWriteable(size_t len); 
    void hasWritten(size_t len); // 写入buffer后移动writeIndex_
    char* beginWrite(); // 返回可写的首个位置指针
    const char* beginWrite() const;
    void append(const char* data, size_t len);
    void append(const void* data, size_t len);
    void append(const std::string& str);
    void append(const Buffer& buff);
    

    ssize_t readFd(int fd, int* Errno);
    ssize_t writeFd(int fd, int* Errno);
};

#endif