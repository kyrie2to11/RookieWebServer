#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>          // open
#include <unistd.h>         // close
#include <sys/stat.h>       // struct stat
#include <sys/mman.h>       // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class httpResponse {
public:
    httpResponse();
    ~httpResponse();

    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void makeResponse(Buffer& buff);
    char* file();
    void unmapFile();
    
    size_t fileLen() const;
    void errorContent(Buffer& buff, std::string message);
    int code() const { return code_; };
private:
    void addStateLine_(Buffer& buff);
    void addHeader_(Buffer& buff);
    void addContent_(Buffer& buff);

    void errorHtml_();
    std::string getFileType_();

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;

    char* mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型集
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 编码状态集
    static const std::unordered_map<int, std::string> CODE_PATH;            // 编码路径集
};
#endif