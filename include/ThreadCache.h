#pragma once
#include "Common.h"

// 线程本地缓存,这是一个单例类
class ThreadCache
{
public:
    static ThreadCache* getInstance()
    {
        static thread_local ThreadCache instance;
        return &instance;
    }

    //主要的两个接口，分配内存和释放内存
    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);

private:
    //单例类，构造私有化
    ThreadCache();

    // 从中心缓存获取内存
    void* fetchFromCentralCache(size_t index);

    // 归还内存到中心缓存
    void returnToCentralCache(void* start, size_t index);

    // 计算批量获取内存块的数量
    size_t getBatchNum(size_t size);

    // 判断是否需要归还内存给中心缓存
    bool shouldReturnToCentralCache(size_t index);

    void freeToLocal(size_t index, void* ptr);

private:
    // 每个线程的自由链表数组，一个静态数组，每个位置记录着链表的开头
    std::array<void*, FREE_LIST_SIZE> freeList_;

    // 自由链表大小统计，每个位置的值表示该位置链表下面挂了多少个可用内存块
    std::array<size_t, FREE_LIST_SIZE> freeListSize_;
};
