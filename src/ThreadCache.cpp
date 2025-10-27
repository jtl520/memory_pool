#include "ThreadCache.h"
#include <iostream>
#include "PageCache.h"
#include "CentralCache.h"

//构造函数
ThreadCache::ThreadCache():freeList_{},freeListSize_{} {}

void* ThreadCache::allocate(size_t size)
{
    // 申请0大小的内存，至少分配一个对齐大小
    if (size == 0)
    {
        size = ALIGNMENT;
    }

    //如果大于256KB，那就直接调用malloc分配
    if (size > MAX_BYTES)
    {
        // 大对象直接从系统分配
        return malloc(size);
    }

    //找到对应的数组的位置
    size_t index = SizeClass::getIndex(size);

    // 检查线程本地自由链表
    // 如果 freeList_[index] 不为空，表示该链表中有可用内存块
    void* ptr = freeList_[index];
    if (ptr)
    {
        // 头指向第二个内存块
        freeList_[index] = *reinterpret_cast<void**>(ptr);
        --freeListSize_[index];
    }else {
        ptr=fetchFromCentralCache(index);
    }

    // 如果线程本地自由链表为空，则从中心缓存获取一批内存
    return ptr;
}

//获取指定index的内存块，每个位置的内存块大小是固定的，8，16，24，32，……
void* ThreadCache::fetchFromCentralCache(size_t index)
{
    //获取要的内存块大小，index=0地方，需要的是字节数为8的内存块
    size_t size = (index + 1) * ALIGNMENT;

    // 根据对象内存大小计算批量获取的数量
    size_t batchNum = getBatchNum(size);

    // 从中心缓存批量获取内存
    void* start = CentralCache::getInstance().fetchRange(index, batchNum);
    if (!start) return nullptr;

    // 统计实际返回的块数（最多 batchNum 个）
    size_t actual = 0;
    void* p = start;
    while (p && actual < batchNum) {
        ++actual;
        p = *reinterpret_cast<void**>(p);
    }

    // 放回本地链 & 正确更新计数
    if (actual > 1) {
        freeList_[index] = *reinterpret_cast<void**>(start);
        freeListSize_[index] += (actual - 1);
    } else {
        freeList_[index] = nullptr; // 只有1个块直接返回给用户
    }

    return start;
}

// 计算批量获取内存块的数量
size_t ThreadCache::getBatchNum(size_t size)
{
    // 基准：每次批量获取不超过4KB内存
    // 根据对象大小设置合理的基准批量数
    size_t baseNum;
    if (size <= 32) baseNum = 128;    // 128 * 32 = 4KB
    else if (size <= 64) baseNum = 64;  // 64 * 64 = 4KB
    else if (size <= 128) baseNum = 32; // 32 * 128 = 4KB
    else if (size <= 256) baseNum = 16;  // 16 * 256 = 4KB
    else if (size <= 512) baseNum = 8;  // 8 * 512 = 4KB
    else if (size <= 1024) baseNum = 4; // 4 * 1024 = 4KB
    else baseNum = 2;                   // 大于1024的对象每次只从中心缓存取1个

    return baseNum;

}


void ThreadCache::deallocate(void* ptr, size_t size)
{
    //大于256KB的，出门右拐
    if (size > MAX_BYTES)
    {
        free(ptr);
        return;
    }

    size_t index = SizeClass::getIndex(size);

    freeToLocal(index, ptr);
}

// 判断是否需要将内存回收给中心缓存
bool ThreadCache::shouldReturnToCentralCache(size_t index)
{
    // 设定阈值，例如：当自由链表的大小超过一定数量时
    // 大块阈值低，小块阈值高
    const size_t size = (index + 1) * ALIGNMENT;

    // 可调常量
    const size_t kBudgetBytes = 64 * 1024; // 每个size-class在线程本地的目标预算
    const size_t kMinBlocks   = 8;         // 最小阈值
    const size_t kMaxBlocks   = 384;      // 最大阈值，保护极小块

    size_t threshold = kBudgetBytes / size;
    if (threshold < kMinBlocks) threshold = kMinBlocks;
    if (threshold > kMaxBlocks) threshold = kMaxBlocks;
    return freeListSize_[index] > threshold;
}


void ThreadCache::returnToCentralCache(void* start, size_t index)
{
    // 计算要归还内存块数量
    size_t batchNum = freeListSize_[index];
    // 如果只有一个块，则不归还
    if (batchNum <= 1) return;

    // 保留一部分在ThreadCache中（比如保留1/4）
    size_t keepNum = std::max(batchNum / 4, size_t(1));
    size_t returnNum = batchNum - keepNum;

    // 将内存块串成链表
    char* current = static_cast<char*>(start);

    // 使用对齐后的大小计算分割点
    char* splitNode = current;
    for (size_t i = 0; i < keepNum - 1; ++i) 
    {
        splitNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(splitNode));
        if (splitNode == nullptr) 
        {
            // 如果链表提前结束，更新实际的返回数量
            returnNum = batchNum - (i + 1);
            break;
        }
    }

    if (splitNode != nullptr) 
    {
        // 将要返回的部分和要保留的部分断开
        void* nextNode = *reinterpret_cast<void**>(splitNode);
        *reinterpret_cast<void**>(splitNode) = nullptr; // 断开连接

        // 更新ThreadCache的空闲链表
        freeList_[index] = start;

        // 更新自由链表大小
        freeListSize_[index] = keepNum;

        // 将剩余部分返回给CentralCache
        if (returnNum > 0 && nextNode != nullptr)
        {
            //返回给中心缓存的链表头，归还块数以及索引index
            CentralCache::getInstance().returnRange(nextNode, returnNum, index);
        }
    }
}

void ThreadCache::freeToLocal(size_t index, void* ptr) {
    *reinterpret_cast<void**>(ptr) = freeList_[index];
    freeList_[index] = ptr;
    ++freeListSize_[index];

    if (shouldReturnToCentralCache(index)) {
        returnToCentralCache(freeList_[index], index); // 注意 returnRange 传“块数”
    }
}
