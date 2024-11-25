#include "webserver.h"
using namespace std;

webServer::webServer(        
        int port, int trigMode, int timeoutMS,
        int sqlPort, const char* sqlUser, const char* sqlPasswd,
        const char* dbName, int connPoolNum, int threadPoolNum,
        bool openLog, int logLevel, bool isAsync):
        port_(port), timeoutMS_(timeoutMS), isClose_(false),
        timer_(new heapTimer()), threadpool_(new threadPool(threadPoolNum)), epoller_(new Epoller()) {
        // 是否打开日志
        if(openLog) {
            Log::getInstance()->init(logLevel, "./webserver_log", ".log", isAsync);

            srcDir_ = getcwd(nullptr, 256);
            assert(srcDir_);
            strcat(srcDir_, "/resources/");
            httpConn::userCount = 0;
            httpConn::srcDir = srcDir_;

            // 初始化SQL连接池(单例模式)
            sqlConnPool::getInstance()->init("localhost", sqlPort, sqlUser, sqlPasswd, dbName, connPoolNum);
            // 初始化事件触发模式
            initEventMode_(trigMode);
            if(!initSocket_()) { isClose_ = true; }

            if(isClose_) {
                LOG_ERROR("================= Server init error! ====================");
            } else {
                LOG_INFO("================= Server init start! ====================");
                LOG_INFO("Listen Mode: %s, Http Connection Mode: %s", listenEvent_ & EPOLLET ? "ET" : "LT", connEvent_ & EPOLLET ? "ET" : "LT");
                LOG_INFO("LogSys Level: %d", logLevel);
                LOG_INFO("Resource Dir: %s", httpConn::srcDir);
                LOG_INFO("sqlConnPool num: %d, threadPool num: %d", connPoolNum, threadPoolNum);
            }

        }
}

webServer::~webServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    sqlConnPool::getInstance()->closePool();
}

void webServer::start() {
    int timeMS = -1; // epoll wait timeout == -1 无事件将阻塞
    if(!isClose_) { LOG_INFO("=============== Server start ================="); }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->getNextTick();
        }
        int eventCnt = epoller_->wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);
            if(fd == listenFd_) {
                dealListen_();
            } else if(events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                closeConn_(&users_[fd]);
            } else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                dealRead_(&users_[fd]);
            } else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                dealWrite_(&users_[fd]);                
            } else {
                LOG_ERROR("Unexpected event!");
            }
        }
    }
}

bool webServer::initSocket_() {
    int ret;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error! port: %d", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 绑定
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 监听
    ret = listen(listenFd_, 8);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->addFd(listenFd_,  listenEvent_ | EPOLLIN);  // 将监听套接字加入epoller
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    setFdNonBlock(listenFd_);   
    LOG_INFO("Server port:%d", port_);
    return true;
}

void webServer::initEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;    // 检测socket关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     // EPOLLONESHOT由一个线程处理
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    httpConn::isET = (connEvent_ & EPOLLET);
}

void webServer::addClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&webServer::closeConn_, this, &users_[fd]));
    }
    epoller_->addFd(fd, EPOLLIN | connEvent_);
    setFdNonBlock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].getFd());
}

// 处理监听套接字，主要逻辑是accept新的套接字，并加入timer和epoller中
void webServer::dealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(httpConn::userCount >= MAX_FD) {
            sendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        addClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

// 处理写事件，主要逻辑是将OnWrite加入线程池的任务队列中
void webServer::dealWrite_(httpConn* client) {
    assert(client);
    extentTime_(client);
    threadpool_->addTask(std::bind(&webServer::onWrite_, this, client));
}

// 处理读事件，主要逻辑是将OnRead加入线程池的任务队列中
void webServer::dealRead_(httpConn* client) {
    assert(client);
    extentTime_(client);
    threadpool_->addTask(std::bind(&webServer::onRead_, this, client)); // 这是一个右值，bind将参数和函数绑定
}

void webServer::sendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void webServer::extentTime_(httpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->getFd(), timeoutMS_); }
}

void webServer::closeConn_(httpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    epoller_->delFd(client->getFd());
    client->closeConn();
}

void webServer::onRead_(httpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);         // 读取客户端套接字的数据，读到httpconn的读缓存区
    if(ret <= 0 && readErrno != EAGAIN) {   // 读异常就关闭客户端
        closeConn_(client);
        return;
    }
    // 业务逻辑的处理（先读后处理）
    onProcess(client);
}

/* 处理读（请求）数据的函数 */
void webServer::onProcess(httpConn* client) {
    // 首先调用process()进行逻辑处理
    if(client->process()) { // 根据返回的信息重新将fd置为EPOLLOUT（写）或EPOLLIN（读）
    //读完事件就跟内核说可以写了
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);    // 响应成功，修改监听事件为写,等待OnWrite_()发送
    } else {
    //写完事件就跟内核说可以读了
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
    }
}

void webServer::onWrite_(httpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->writeBytesLen() == 0) {
        /* 传输完成 */
        if(client->isKeepAlive()) {
            // OnProcess(client);
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN); // 回归换成监测读事件
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {  // 缓冲区满了 
            /* 继续传输 */
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    closeConn_(client);
}

// 设置非阻塞
int webServer::setFdNonBlock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}