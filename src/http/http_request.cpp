#include "http_request.h"
using namespace std;

// 网页名称
const unordered_set<string> httpRequest::DEFAULT_HTML {
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

// 登录/注册
const unordered_map<string, int> httpRequest::DEFAULT_HTML_TAG {
    {"/login.html", 1}, {"/register.html", 0},
};

// 初始化
void httpRequest::init() {
    state_ = REQUEST_LINE;
    method_ = path_ = version_ = body_ = "";
    header_.clear();
    post_.clear();
}

// 解析处理
bool httpRequest::parse(Buffer& buff) {
    const char END[] = "\r\n"; // 结束符：回车、换行
    if(buff.readableBytes() == 0) return false; // 无可读数据
    // 读取数据开始
    while(buff.readableBytes() && state_ != FINISH) {
        // 从buffer中读指针开始到写指针结束（前闭后开），并去除"\r\n"，返回有效数据的行末指针
        const char* beginwrite = buff.beginWrite();
        const char* lineend = search(buff.peek(), beginwrite , END, END + 2); // 查找 [first1, last1) 范围内第一个 [first2, last2) 子序列
        string line(buff.peek(), lineend);
        switch(state_) {
            case REQUEST_LINE:
                // 解析错误
                if(!parseRequestLine_(line)) {
                    return false;
                }
                parsePath_(); // 解析路径
                break;
            case HEADERS:
                parseHeader_(line);
                if(buff.readableBytes() <= 2) { // 说明是get请求
                    state_ = FINISH; // 提前结束
                }
                break;
            case BODY:
                parseBody_(line);
                break;
            default:
                break;
        }
        if(lineend == buff.beginWrite()) { // 读完了
            buff.retrieveAll();
            break;
        }
        buff.retrieveUntil(lineend + 2); // 跳过回车换行符
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

bool httpRequest::parseRequestLine_(const string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); // 匹配正则表达式，以括号（）划分组别，一共三组分别匹配：请求方法（GET、POST）; URL 资源路径; 协议版本
    smatch matches; // 用来匹配patten得到的结果
    if(regex_match(line, matches, patten)) {
        method_ = matches[1];
        path_ = matches[2];
        version_ = matches[3];
        state_ = HEADERS; // 切换下一个状态
        return true;
    }
    LOG_ERROR("RequestLine Parse Error!");
    return false;
}

void httpRequest::parseHeader_(const string& header) {
    regex patten("^([^:]*): ?(.*)$");
    smatch matches;
    if(regex_match(header, matches, patten)) {
        header_[matches[1]] = matches[2];
    } else {    // 匹配失败说明请求头，单行匹配完成，进行状态转变
        state_ = BODY;
    }
}

void httpRequest::parseBody_(const string& body) {
    body_ = body;
    parsePost_();
    state_ = FINISH; // 解析请求数据完毕，状态设置为完成
    LOG_DEBUG("Body:%s, len:%d", body.c_str(), body.size());
}

// 解析路径，统一path_名称后缀.html
void httpRequest::parsePath_() {
    if(path_ == "/") {
        path_ = "/index.html";
    } else {
        if(DEFAULT_HTML.find(path_) != DEFAULT_HTML.end()) {
            path_ += ".html";
        }
    }
}

// 处理post请求
void httpRequest::parsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        parseFromUrlEncoded_(); // POST请求体示例
        if(DEFAULT_HTML_TAG.count(path_)) { // 如果是登录/注册的path
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag: %d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1); // 为1则是登录
                if(userVerify_(post_["username"], post_["passwd"], isLogin)) {
                    path_ = "/welcome.html";
                } else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

// 从url中解析编码
void httpRequest::parseFromUrlEncoded_() {
    if(body_.size() == 0) {return; }
    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++){
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = convertHexToDecimal_(body_[i + 1]) * 16 + convertHexToDecimal_(body_[i + 2]);
            body_[i + 2] = num % 10 + '0'; // 利用 num + '0' 将整数num转换为对应的字符
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break; 
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool httpRequest::userVerify_(const std::string& name, const std::string& passwd, bool isLogin) {
    // 1. 检查用户名或密码是否为空
    if(name == "" || passwd == "") {
        return false;
    }

    LOG_INFO("Verify Name: %s, Password: %s", name.c_str(), passwd.c_str());

    // 2. 获取数据库连接
    MYSQL* sql;
    sqlConnRAII(&sql, sqlConnPool::getInstance());
    assert(sql);

    // 构建通用的查询语句相关变量
    char order[256] = {0};
    MYSQL_RES* res = nullptr;

    // 构建查询用户名和密码的SQL语句
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("SQL: %s", order);

    if(mysql_query(sql, order)) {  // 如果查询失败
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);

    // 登录或注册
    if(isLogin) {  // 登录逻辑
        if(res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                std::string password(row[1]);
                if (passwd == password) {
                    LOG_INFO("Password Correct!");
                    return true;
                } else {
                    LOG_INFO("Password Error!");
                    return false;
                }
            }
            mysql_free_result(res);
        }
    } else {  // 注册逻辑
        // 先查询用户名是否已存在
        bool usernameExists = (res && mysql_num_rows(res) > 0);
        mysql_free_result(res);
        if (usernameExists) {
            LOG_INFO("Username used!");
            return false;
        }

        // 用户名不存在，执行插入注册信息的操作
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), passwd.c_str());
        LOG_DEBUG( "SQL: %s", order);

        if (mysql_query(sql, order)) {
            LOG_DEBUG( "Insert error!");
            return false;
        }

        LOG_INFO("Register success!");
        return true;
    }
    // sqlConnPool::getInstance()->freeConn(sql); 因为 sqlConnRAII 析构函数中调用了freeConn
    return true;
}

// 16进制单字符转10进制int
int httpRequest::convertHexToDecimal_(char ch) {
    if(ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

string httpRequest::path() const{
    return path_;
}

string& httpRequest::path() {
    return path_;
}

string httpRequest::method() const{
    return method_;
}

string httpRequest::version() const{
    return version_;
}

string httpRequest::getPost(const string& key) const{
    assert(key != "");
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }
    return "";
}

string httpRequest::getPost(const char* key) const{
    assert(key != nullptr);
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }
    return "";
}

bool httpRequest::isKeepAlive() const{
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}
