//
// Created by 86150 on 2025/9/19.
//
#ifndef TXKJ_MYMEMORY_H
#define TXKJ_MYMEMORY_H
#include <stddef.h>  //NULL
#include <cstddef>
#include <cstdint>
#include "MemoryPool.h"

struct alignas(std::max_align_t) MpHeader {
    std::size_t user_size;   // 用户区大小（未对齐原始值）
    std::uint32_t magic;     // 简单校验，防误释放
    std::uint32_t flags;     // 预留：对齐/调试/来源等
};
//用于校验
static constexpr std::uint32_t kMpMagic = 0xC0FFEEu;

//内存相关类
class CMemory
{
private:
    CMemory() {}  //构造函数，因为要做成单例类，所以是私有的构造函数

public:
    ~CMemory(){};
    CMemory(const CMemory&)=delete;
    CMemory& operator=(const CMemory&)=delete;
public:
    static CMemory* GetInstance() //单例
    {
        static CMemory instance;
        return &instance;
    }
public:
    void *AllocMemory(int memCount,bool ifmemset);
    void FreeMemory(void *point);

};

#endif //TXKJ_MYMEMORY_H