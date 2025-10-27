#pragma once
#include <cstddef>
#include <atomic>
#include <algorithm>
#include <array>

// 对齐数大小
constexpr size_t ALIGNMENT = 16;

//一个界限，大于64KB就调用系统malloc，小于就用内存池
constexpr size_t MAX_BYTES = 256 * 1024;

// 每次从PageCache获取span大小（以页为单位）
 constexpr size_t SPAN_PAGES = 8;

 constexpr size_t PAGE_SIZE = 4096; // 4K页大小

//constexpr size_t SMALL_MAX = SPAN_PAGES * PAGE_SIZE; // 8页 * 4KB = 32KB

constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT等于指针void*的大小

// 大小类管理
class SizeClass 
{
public:
    //其实就是把要分配的内存，对齐到比它大的内存块，方便分配
    //如 申请1字节，就会分配8字节；申请12字节，就会分配16字节；真正分配的是对齐数的整数倍
    //这能很好的避免外部碎片，但是无法避免内部碎片
    static size_t roundUp(size_t bytes)
    {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    //这是根据申请的内存大小，找到数组对应的位置
    //如，申请1个字节，实际上是分配了8个字节，而这个8个字节的内存块，全部都在第一个位置，也就是index=0的位置
    static size_t getIndex(size_t bytes)
    {   
        // 确保bytes至少为ALIGNMENT
        bytes = std::max(bytes, ALIGNMENT);
        // 向上取整后-1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};
