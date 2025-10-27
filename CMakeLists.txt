cmake_minimum_required(VERSION 3.10)
project(memory_pool)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局告警（可保留）
add_compile_options(-Wall)

# 定义开关（默认 OFF）,也就是默认是优化的，检测必须关闭优化
option(DISABLE_OPTIMIZATIONS "Force -O0 -g for the whole project" OFF)

if (DISABLE_OPTIMIZATIONS)
    if (MSVC)
        # MSVC 下的等价选项：/Od 关闭优化；/Zi 生成 PDB；/Ob0 禁止内联；/Oy- 保留帧指针
        add_compile_options(/Od /Zi /Ob0 /Oy-)
        add_link_options(/DEBUG)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # Clang/GCC：关闭优化 + 调试符号 + 保留帧指针（便于 gdb/valgrind）
        add_compile_options(-O0 -g -fno-omit-frame-pointer)
        add_link_options(-fno-omit-frame-pointer)
        # 如需更易读的调用栈，可（可选）再打开下面一行，但会更慢：
        # add_compile_options(-fno-inline)
    endif()
endif()

option(ENABLE_ASAN "memory-test" OFF)

if (ENABLE_ASAN)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-O1 -g -fno-omit-frame-pointer -fsanitize=address)
        add_link_options(-fsanitize=address)
        # 如需一起查 UB/泄漏：
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=undefined)
        add_compile_options(-fsanitize=leak)
        add_link_options(-fsanitize=leak)
    elseif (MSVC)
        add_compile_options(/fsanitize=address /Zi)
        # 某些版本建议禁用增量链接，避免符号化异常
        add_link_options(/INCREMENTAL:NO)
    endif()
endif()

# 查找pthread库
find_package(Threads REQUIRED)

# 设置目录
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_SOURCE_DIR}/include)
set(TEST_DIR ${CMAKE_SOURCE_DIR}/tests)

# 源文件
file(GLOB SOURCES "${SRC_DIR}/*.cpp")

# 添加头文件目录
include_directories(${INC_DIR})

# 创建单元测试可执行文件
add_executable(unit_test 
    ${SOURCES}
    ${TEST_DIR}/UnitTest.cpp
        src/mymemory.cpp
        include/mymemory.h
)

# 创建性能测试可执行文件
add_executable(perf_test
    ${SOURCES}
    ${TEST_DIR}/PerformanceTest.cpp
        src/mymemory.cpp
        include/mymemory.h
)

# 链接pthread库
target_link_libraries(unit_test PRIVATE Threads::Threads)
target_link_libraries(perf_test PRIVATE Threads::Threads)

# 添加测试命令
add_custom_target(test
    COMMAND ./unit_test
    DEPENDS unit_test
)

add_custom_target(perf
    COMMAND ./perf_test
    DEPENDS perf_test
)