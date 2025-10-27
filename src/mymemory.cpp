//
// Created by 86150 on 2025/9/20.
//


//和 内存分配 有关的函数放这里

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mymemory.h"

#include <cstring>

void* CMemory::AllocMemory(int memCount, bool ifmemset) {
    if (memCount < 0) return nullptr;
    std::size_t u = static_cast<std::size_t>(memCount);

    // 申请：头部 + 用户区
    std::size_t raw = sizeof(MpHeader) + u;
    void* raw_ptr = MemoryPool::allocate(raw);
    if (!raw_ptr) throw std::bad_alloc{};

    // 写头部
    auto* hdr = reinterpret_cast<MpHeader*>(raw_ptr);
    hdr->user_size = u;
    hdr->magic     = kMpMagic;
    hdr->flags     = 0;

    // 返回用户指针
    void* user = reinterpret_cast<void*>(hdr + 1);
    if (ifmemset) std::memset(user, 0, u);
    return user;
}

void CMemory::FreeMemory(void* p) {
    if (!p) return;

    // 还原头部
    auto* hdr = reinterpret_cast<MpHeader*>(p) - 1;

    // 简单校验，避免误释放外部指针
    if (hdr->magic != kMpMagic) {
        return;
    }

    // 计算 raw 指针与真实大小（头部 + 用户区）
    void* raw_ptr = static_cast<void*>(hdr);
    std::size_t raw_size = sizeof(MpHeader) + hdr->user_size;

    // 走带尺寸快路径（关键！）
    MemoryPool::deallocate(raw_ptr, raw_size);
}

