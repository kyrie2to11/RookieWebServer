#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>              // fcntl()
#include <unistd.h>             // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../timer/heap_timer.h"
#include "../log/log.h"
#include "../pool/sqlconn_pool.h"
#include "../pool/thread_pool.h"
#include "../http/http_conn.h"

class webServer {
public:
    webServer(
        int port, int trigMode, int timeoutMS,
        int sqlPort, const char* sqlUser, const char* sqlPasswd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize
    );
    ~webServer();
    void start();

private:
    bool initSocket_();
    void initEventMode_(int trigMode);
    void addClient_(int fd, sockaddr_in addr);

    void dealListen_();
    void dealWrite_(httpConn* client);
    void dealRead_(httpConn* client);

    void sendError_(int fd, const char* info);
    void extentTime_(httpConn* client);
    void closeConn_(httpConn* client);

    void onRead_(httpConn* client);
    void onWrite_(httpConn* client);
    void onProcess(httpConn* client);

    static const int MAX_FD = 65536;
    static int setFdNonBlock(int fd);

    int port_;
    // bool openLinger_;
    int timeoutMS_; // 毫秒 MS
    bool isClose_;
    int listenFd_;
    char* srcDir_;

    uint32_t listenEvent_; // 监听事件
    uint32_t connEvent_;   // 连接事件

    std::unique_ptr<heapTimer> timer_;
    std::unique_ptr<threadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, httpConn> users_;

};

#endif