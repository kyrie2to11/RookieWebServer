# 设置项目名称
project(server)

# 设置最低要求的 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 设置 C++ 标准为 C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找线程库
find_package(Threads REQUIRED)

# 查找 MySQL 客户端库，根据实际安装情况调整路径等配置
find_library(MYSQL_CLIENT_LIB mysqlclient)

# 收集所有源文件，使用 glob 函数来匹配各个目录下的.cpp 文件
file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/log/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/pool/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/timer/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/http/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/server/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/buffer/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
)

# 添加可执行文件目标
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# 添加编译选项，对应之前 Makefile 中的 -Wall 和 -g
add_compile_options(-Wall -g)

# 添加优化级别选项，对应之前 Makefile 中的 -O2
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# 链接线程库
target_link_libraries(${PROJECT_NAME} Threads::Threads)

# 链接 MySQL 客户端库
target_link_libraries(${PROJECT_NAME} ${MYSQL_CLIENT_LIB})

# 设置生成的可执行文件输出到 bin 目录下
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)

# 以下部分是为了方便在 VS Code 中进行调试配置的内容

# 检查是否处于调试模式（如果是使用 VS Code 的调试功能会设置这个变量）
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # 添加调试相关的编译选项，确保调试信息完整
    target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g)
endif()