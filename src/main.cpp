#include <unistd.h>
#include "./server/webserver.h"

int main() {
    // 守护进程 后台运行
    webServer server(
        1316, 3, 60000,              // 端口 ET模式 timeoutMs 
        3306, "root", "qq105311", "mydb", /* mysql配置 */
        12, 8, true, 1, 1024);             /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    server.start();
    
    return 0;
} 