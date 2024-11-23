#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <atomic>       // std::atomic<int> userCount;
#include <sys/types.h>
#include <sys/uio.h>    // readv/writev
#include <arpa/inet.h>  // sockaddr_in
#include <stdlib.h>     // atoi() 字符串转换为整数
#include <errno.h>


#include "../log/log.h"
#include "../buffer/buffer.h"
#include "http_request.h"
#include "http_response.h"

// 进行读写数据并调用httprequest解析数据以及调用httpresponse来生成响应
class httpConn {
public:
    httpConn();
    ~httpConn();

    void init(int sockFd, const sockaddr_in& addr);
    void closeConn();
    int getFd() const;
    int getPort() const;
    sockaddr_in getAddr() const;
    const char* getIP() const;
    
    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);
    bool process();

    // 写的总长度
    int writeBytesLen() {
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    bool isKeepAlive() const {
        return request_.isKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount; // 原子，支持锁

    
private:
    int fd_;
    struct sockaddr_in addr_;
    bool isClose_;
    int iovCnt_;
    struct iovec iov_[2];

    Buffer readBuff_; // 读缓冲区
    Buffer writeBuff_; // 写缓冲区

    httpRequest request_;
    httpResponse response_;

};

#endif