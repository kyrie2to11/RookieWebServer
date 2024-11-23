#include "buffer.h"

// vector<char> buffer_初始化， 读写下标初始化
Buffer::Buffer(size_t initBuffSize, size_t cheapPrepend)
    : buffer_(cheapPrepend + initBuffSize),
      readIndex_(cheapPrepend),
      writeIndex_(cheapPrepend),
      kCheapPrepend_(cheapPrepend),
      kInitBuffSize_(initBuffSize)
{
    assert(readableBytes() == 0);
    assert(writableBytes() == initBuffSize);
    assert(prependableBytes() == cheapPrepend);

}

// 可写的数量：buffer大小 - 写下标
size_t Buffer::writableBytes() const {
    return buffer_.size() - writeIndex_;
}

// 可读的数量：写下标 - 读下标
size_t Buffer::readableBytes() const {
    return writeIndex_ - readIndex_;
}

// 预留空间大小
size_t Buffer::prependableBytes() const {
    return readIndex_;
}

// 返回读位置指针
const char* Buffer::peek() const {
    return &buffer_[readIndex_];
}

// 读取len长度后，移动读下标
void Buffer::retrieve(size_t len) {
    assert(len <= readableBytes());
    if(len < readableBytes()) {
        readIndex_ += len;
    } else {
        retrieveAll();
    }
}

// 读取到end位置后，移动读下标
void Buffer::retrieveUntil(const char* end) {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek()); // end指针 - 读指针 长度
}

// 取出所有数据后读写下标复位
void Buffer::retrieveAll() {
    readIndex_ = kCheapPrepend_;
    writeIndex_ = kCheapPrepend_;
}

// 取出指定长度str
std::string Buffer::retrieveAsString(size_t len) {
    assert(len <= readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

// 取出所有可读str
std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

// 确保可写的长度满足len要求，若不够则扩容
void Buffer::ensureWriteable(size_t len) {
    if(len > writableBytes()) {
        makeSpace_(len);
    }
    assert(len <= writableBytes());
}

// 移动写下标，在append中使用
void Buffer::hasWritten(size_t len) {
    writeIndex_ += len;
}

// 返回写指针位置
char* Buffer::beginWrite() {
    return &buffer_[writeIndex_];
}

const char* Buffer::beginWrite() const {
    return &buffer_[writeIndex_];
}

// 添加str到缓冲区
void Buffer::append(const char* data, size_t len) {
    ensureWriteable(len); // 确保可写长度
    std::copy(data, data + len, beginWrite()); // 将data[data, data + len)放到写下标开始的地方
    hasWritten(len); // 移动写下标
}

void Buffer::append(const void* data, size_t len) {
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const std::string& str) {
    append(str.c_str(), str.size()); // str.c_str() 返回值类型为 const char*
}

// 将buffer中的可读内容放到buffer可写区域
void Buffer::append(const Buffer& buff) {
    append(buff.peek(), buff.readableBytes());
}

// 将fd的内容读到缓冲区，即writeable的位置
ssize_t Buffer::readFd(int fd, int* Errno) {
    char buff[65536]; // 栈区
    struct iovec vec[2];
    size_t writeable = writableBytes(); // 先记录能写多少
    //分散读，保证数据全部读完
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writeable;
    vec[1].iov_base = buff;
    vec[1].iov_len = sizeof(buff); 

    ssize_t len = readv(fd, vec, 2); // 先调用 readv 读取到 buffer_ 和 buff
    if(len < 0) {
        *Errno = errno;
    } else if(static_cast<size_t>(len) <= writeable) { // 若len小于writable，说明写区可以容纳len
        hasWritten(len); // 移动写下标
    } else {
        writeIndex_ = buffer_.size(); // buffer_写区写满了,下标移到最后
        append(buff, static_cast<size_t>(len - writeable)); // 将buff中的部分再写到buffer_中，调用的append(const char* data, size_t len)
    }
    return len;
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::writeFd(int fd, int* Errno) {
    ssize_t len = write(fd, peek(), readableBytes());
    if(len < 0) {
        *Errno = errno;
        return len;
    } 
    retrieve(len);
    return len;
}

char* Buffer::begin_() {
    return &buffer_[0];
}

const char* Buffer::begin_() const{
    return &buffer_[0];
}

// 扩展空间
void Buffer::makeSpace_(size_t len) {
    if(writableBytes() + prependableBytes() < len + kCheapPrepend_) {
        buffer_.resize(writeIndex_ + len);
    } else {
        size_t readable = readableBytes();
        std::copy(begin_() + readIndex_, begin_() + writeIndex_, begin_() + kCheapPrepend_);
        readIndex_ = kCheapPrepend_;
        writeIndex_ = readIndex_ + readable;
        assert(readable == readableBytes());
    }
}
