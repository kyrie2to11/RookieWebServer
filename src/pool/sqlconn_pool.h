#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <cassert>
#include <semaphore.h>
#include "../log/log.h"


class sqlConnPool {
public:
    static sqlConnPool* getInstance(); // 单例模式
    MYSQL* getConn(); // 从sql链接池中获取sql链接
    void freeConn(MYSQL* conn); // 释放sql链接回归链接池
    int getFreeConnCount(); // 获取sql链接池中目前有多少空闲链接

    void init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int connSize);
    void closePool();

private:
    sqlConnPool() = default;
    ~sqlConnPool() {closePool();}

    int MAX_CONN_;
    std::queue<MYSQL*> connQue_;
    std::mutex mtx_;
    sem_t semId_; // 信号量
};

/* 资源在对象构造初始化 资源在对象析构时释放*/
class sqlConnRAII {
public:
    sqlConnRAII(MYSQL** sql, sqlConnPool* connpool) {
        assert(connpool);
        *sql = connpool->getConn();
        sql_ = *sql;
        connpool_ = connpool;
    }

    ~sqlConnRAII() {
        if(sql_) {
            connpool_->freeConn(sql_);
        }
    }

private:
    MYSQL* sql_;
    sqlConnPool* connpool_;

};
#endif