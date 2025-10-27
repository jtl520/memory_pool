//
// Created by 86150 on 2025/9/19.
//
#ifndef TXKJ_MYMEMORY_H
#define TXKJ_MYMEMORY_H

#include <cstddef>
#include <cstdint>
#include "MemoryPool.h"

// 可选：开发/CI 打开校验，Release 关闭即可免分支
// #define MP_DEBUG_GUARD 1

// 16B 对齐的最小头部：记录 class 索引 + 回退偏移 + 用户原始大小
// sizeof == 16，align == 16，保证 (hdr+1) 也保持 16B 对齐
struct alignas(16) MpHeader {
    std::uint16_t size_class;    // 小对象释放 O(1)
    std::uint8_t  back;          // base→hdr 的对齐回退偏移（字节，<16）
    std::uint8_t  flags;         // bit0: LARGE（>MAX_BYTES 的直通大对象）
#ifdef MP_DEBUG_GUARD
    std::uint32_t magic;         // 调试校验
#else
    std::uint32_t pad;           // 填充保持 16B 头部
#endif
    std::uint64_t user_size;     // 仅大对象路径使用；小对象不依赖它
};
static_assert(sizeof(MpHeader) == 16, "MpHeader must be 16 bytes");

#ifdef MP_DEBUG_GUARD
static constexpr std::uint32_t kMpMagic = 0xC0FFEEu;
#endif

// 单例分配器外壳（保持你原来的接口）
class CMemory {
private:
    CMemory() = default;

public:
    ~CMemory() = default;
    CMemory(const CMemory&) = delete;
    CMemory& operator=(const CMemory&) = delete;

    static CMemory* GetInstance() {
        static CMemory instance;
        return &instance;
    }

    void* AllocMemory(int memCount, bool ifmemset);
    void  FreeMemory(void* point);
};

#endif // TXKJ_MYMEMORY_H
