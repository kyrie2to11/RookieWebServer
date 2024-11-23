#include "sqlconn_pool.h"

// 懒汉式单例 局部静态变量法 （这种方法不需要加锁解锁操作）
sqlConnPool* sqlConnPool::getInstance() {
    static sqlConnPool pool;
    return &pool;
}

// 初始化
void sqlConnPool::init(const char* host, int port,
                       const char* user, const char* pwd,
                       const char* dbName, int connSize = 10) {
    assert(connSize > 0);
    for(int i = 0; i < connSize; i++) {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if(!conn) {
            LOG_ERROR("MySQL init error!");
            assert(conn);
        }
        conn = mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0);
        if(!conn) {
            LOG_ERROR("MySQL connect error!");
        }
        connQue_.emplace(conn);
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);

}

MYSQL* sqlConnPool::getConn() {
    MYSQL* conn = nullptr;
    if(connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_); // -1
    lock_guard<mutex> locker(mtx_);
    conn = connQue_.front();
    connQue_.pop();
    return conn;
}

// 存入连接池，实际上没有关闭
void sqlConnPool::freeConn(MYSQL* conn) {
    assert(conn);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(conn);
    sem_post(&semId_); // +1
}

void sqlConnPool::closePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto conn = connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }
    mysql_library_end();
}

int sqlConnPool::getFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}