#include "CentralCache.h"
#include "PageCache.h"
#include <cassert>
#include <thread>

void* CentralCache::fetchRange(size_t index, size_t batchNum)
{
    // 索引检查，当索引大于等于FREE_LIST_SIZE时，说明申请内存过大应直接向系统申请
    if (index >= FREE_LIST_SIZE || batchNum == 0) 
        return nullptr;

    // 自旋锁保护
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
    }

    //拿到了锁
    void* result = nullptr;
    try 
    {
        // 尝试从中心缓存获取内存块
        result = centralFreeList_[index].load(std::memory_order_acquire);

        if (!result)
        {
            // 如果中心缓存为空，从页缓存获取新的内存块
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);

            if (!result)
            {
                //释放锁
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            size_t numPages = (size <= SPAN_PAGES * PAGE_SIZE)
                ? SPAN_PAGES
                : ( (size + PAGE_SIZE - 1) / PAGE_SIZE );

            // 将从PageCache获取的内存块切分成小块
            char* start = static_cast<char*>(result);
            size_t totalBlocks = (numPages * PAGE_SIZE) / size;

            //分配小的，那多余部分咋办
            size_t allocBlocks = std::min(batchNum, totalBlocks);

            if (allocBlocks == 1) {
                *reinterpret_cast<void**>(start) = nullptr;
            }

            // 构建返回给ThreadCache的内存块链表
            if (allocBlocks > 1) 
            {  
                // 确保至少有两个块才构建链表
                // 构建链表
                for (size_t i = 1; i < allocBlocks; ++i) 
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                //最后一个结点指向空
                *reinterpret_cast<void**>(start + (allocBlocks - 1) * size) = nullptr;
            }

            // 构建保留在CentralCache的链表，这里处理多余部分
            if (totalBlocks > allocBlocks)
            {
                void* remainStart = start + allocBlocks * size;
                //把多余的结点也串起来
                for (size_t i = allocBlocks + 1; i < totalBlocks; ++i)
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (totalBlocks - 1) * size) = nullptr;

                //多余的挂在中心缓存里
                centralFreeList_[index].store(remainStart, std::memory_order_release);
            }
        } 
        else // 如果中心缓存有index对应大小的内存块
        {
            // 从现有链表中获取指定数量的块
            void* current = result;
            void* prev = nullptr;
            size_t count = 0;

            while (current && count < batchNum)
            {
                prev = current;
                current = *reinterpret_cast<void**>(current);
                count++;
            }

            //问题又来了，多余的咋办
            if (prev) // 当前centralFreeList_[index]链表上的内存块大于batchNum时需要用到 
            {
                *reinterpret_cast<void**>(prev) = nullptr;
            }

            centralFreeList_[index].store(current, std::memory_order_release);
        }
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    // 释放锁
    locks_[index].clear(std::memory_order_release);
    return result;
}



void* CentralCache::fetchFromPageCache(size_t size)
{
    // 1. 计算实际需要的页数，size最小是8，8-4k都是分配一页，以此类推
    size_t numPages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // 2. 根据大小决定分配策略
    if (size <= SPAN_PAGES * PAGE_SIZE)
    {
        // 小于等于32KB的请求，使用固定8页
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    }
    else
    {
        // 大于32KB的请求，按实际需求分配
        return PageCache::getInstance().allocateSpan(numPages);
    }
}



void CentralCache::returnRange(void* start, size_t returnNum, size_t index)
{
    // 当索引大于等于FREE_LIST_SIZE时，说明内存过大应直接向系统归还
    if (!start || index >= FREE_LIST_SIZE) 
        return;

    while (locks_[index].test_and_set(std::memory_order_acquire)) 
    {
        std::this_thread::yield();
    }

    try 
    {
        // 找到要归还的链表的最后一个节点
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr && count < returnNum) {
            end = *reinterpret_cast<void**>(end);
            count++;
        }

        // 将归还的链表连接到中心缓存的链表头部
        void* current = centralFreeList_[index].load(std::memory_order_acquire);
        *reinterpret_cast<void**>(end) = current;  // 将原链表头接到归还链表的尾部
        centralFreeList_[index].store(start, std::memory_order_release);  // 将归还的链表头设为新的链表头
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}
