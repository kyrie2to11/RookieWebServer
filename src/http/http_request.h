#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <regex> // 正则表达式
#include <algorithm> // search
#include <errno.h>
#include <mysql/mysql.h> // mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconn_pool.h"

class httpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    httpRequest() {init();}
    ~httpRequest() = default;

    void init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;

    bool isKeepAlive() const;

private:
    bool parseRequestLine_(const std::string& line); // 处理请求行
    void parseHeader_(const std::string& header);    // 处理请求头部字段
    void parseBody_(const std::string& body);        // 处理请求体

    void parsePath_();                               // 处理请求路径
    void parsePost_();                               // 处理Post事件
    void parseFromUrlEncoded_();                     // 从url解析编码

    static bool userVerify_(const std::string& name, const std::string& passwd, bool isLogin); // 用户验证
    static int convertHexToDecimal_(char ch); // 16进制转10进制

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    
};

#endif