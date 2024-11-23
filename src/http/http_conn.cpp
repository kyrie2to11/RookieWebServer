#include "http_conn.h"

// 在类外对静态成员初始化
const char* httpConn::srcDir;
std::atomic<int> httpConn::userCount;
bool httpConn::isET;

httpConn::httpConn() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

httpConn::~httpConn() {
    closeConn();
}

void httpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.retrieveAll();
    readBuff_.retrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in , userCount:%d", fd_, getIP(), getPort(), (int)userCount);
}

void httpConn::closeConn() {
    response_.unmapFile();
    if(isClose_ == false) {
        isClose_ = true;
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit , userCount:%d", fd_, getIP(), getPort(), (int)userCount);
    }
}

int httpConn::getFd() const {
    return fd_;
}

int httpConn::getPort() const {
    return addr_.sin_port;
}

sockaddr_in httpConn::getAddr() const {
    return addr_;
}
const char* httpConn::getIP() const {
    return inet_ntoa(addr_.sin_addr); // 将网络字节序表示的 IPv4 地址转换为点分十进制表示的字符串形式
}

ssize_t httpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do{
        len = readBuff_.readFd(fd_, saveErrno);
        if(len <= 0){
            break;
        }
    } while(isET); // ET: 边沿触发要一次性全部读出
    return len;
}

// 采用writev集中写函数（将多个缓冲区数据或文件写入同一处）
ssize_t httpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_); // 将iov_[0], iov_[1]中的内容写到fd_中
        if(len <= 0) {
            *saveErrno = errno; // errno 是一个全局变量，用于记录系统调用最后一次出错的错误码
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len == 0) { // 传输结束
            break;
        } else if(static_cast<size_t>(len) > iov_[0].iov_len) { // 传输数据量大于iov_[0] (Buffer) 的数据量
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len); // iov_base 类型为 void* 通用指针，不能进行算术运算
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.retrieveAll(); // iov_[0] 放的是 writeBuff_
                iov_[0].iov_len = 0;
            }
        } else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.retrieve(len); 
        }
    } while(isET || writeBytesLen() > 13200); // 13200 = (8 + 1024) * 10; 8 = kCheapPrepend, 1024 = initBuffSize 
    return len;
}

bool httpConn::process() {
    request_.init();
    if(readBuff_.readableBytes() <= 0) {
        return false;
    } else if(request_.parse(readBuff_)) { // 解析成功
        LOG_DEBUG("%s", request_.path().c_str());
        response_.init(srcDir, request_.path(), request_.isKeepAlive(), 200);
    } else {
        response_.init(srcDir, request_.path(), false, 400);
    }

    response_.makeResponse(writeBuff_); // 生成响应写入writeBuff_中
    // 响应头
    iov_[0].iov_base = const_cast<char*>(writeBuff_.peek());
    iov_[0].iov_len = writeBuff_.readableBytes();
    iovCnt_ = 1;

    // 文件
    if(response_.fileLen() > 0 && response_.file()) {
        iov_[1].iov_base = response_.file();
        iov_[1].iov_len = response_.fileLen();
        iovCnt_ = 2;
    }
    LOG_DEBUG("File size: %dB(response header) + %dB(content) = %dB", iov_[0].iov_len, iov_[1].iov_len, writeBytesLen());
    return true;
}
