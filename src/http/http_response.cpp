#include "http_response.h"

using namespace std;

const unordered_map<string, string> httpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> httpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> httpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

httpResponse::httpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

httpResponse::~httpResponse() {
    unmapFile();
}

void httpResponse::init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code) {
    assert(srcDir != "");
    if(mmFile_) { unmapFile();}
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = { 0 };
}

void httpResponse::makeResponse(Buffer& buff) {
    /* 判断请求的资源文件 */
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) { // 如果路径对应的文件不存在或者对应的路径是目录
        code_ = 404; // 请求的资源未找到
    } else if(!(mmFileStat_.st_mode & S_IROTH)) { // 如果请求者没有读权限
        code_ = 403; // 禁止访问
    } else if(code_ == -1) {
        code_ == 200; // 请求成功
    }
    errorHtml_(); // 返回错误编码对应文件路径状态到mmFileStat_
    addStateLine_(buff);
    addHeader_(buff);
    addContent_(buff);
}

char* httpResponse::file() {
    return mmFile_;
}

void httpResponse::unmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

size_t httpResponse::fileLen() const{
    return mmFileStat_.st_size;
}

void httpResponse::errorContent(Buffer& buff, std::string message) {
    string body, status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body +=  to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>RookieWebServer</em></body></html>";

    buff.append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}

void httpResponse::addStateLine_(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        code_ = 400;
        status = CODE_STATUS.find(code_)->second;
    }
    buff.append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void httpResponse::addHeader_(Buffer& buff) {
    buff.append("Connection: ");
    if(isKeepAlive_) {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.append("close\r\n");
    }
    buff.append("Content-type: " + getFileType_() + "\r\n");
}

void httpResponse::addContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) {
        errorContent(buff, "File NotFound!");
        return;
    }

    // 将文件映射到内存提高文件访问速度 MAP_PRIVATE 建立一个写入时拷贝的私有映射
    LOG_DEBUG("file path: %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        errorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char*) mmRet;
    close(srcFd);
    buff.append("Content-legth: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void httpResponse::errorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

// 判断 mmFile_ 文件类型
string httpResponse::getFileType_() {
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos) { // 最大值find函数再找不到指定值的情况下返回string::npos
        return "text/plain";
    }
    string suffix = path_.substr(idx); // 截取文件名后缀
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";

}
