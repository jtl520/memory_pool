#pragma once
#include "Common.h"
#include <map>
#include <mutex>
#include <cstdint>
#include <shared_mutex>

class PageCache
{
public:
    struct Span
    {
        void*  pageAddr; // 页起始地址
        size_t numPages; // 页数
        Span*  next;     // 链表指针
    };

public:
    static PageCache& getInstance()
    {
        static PageCache instance;
        return instance;
    }

    // 分配指定页数的span
    void* allocateSpan(size_t numPages);

    // 释放span
    void deallocateSpan(void* ptr, size_t numPages);

    //Span* findSpan(void* anyPtr);

    ~PageCache();              // ← 声明析构
    void shutdown();           // ← 也提供显式清理接口（见下）

private:
    PageCache() = default;

    // 向系统申请内存
    void* systemAlloc(size_t numPages);

    // 按页数管理空闲span，不同页数对应不同Span链表
    std::map<size_t, Span*> freeSpans_;

    // 页起始地址到span的映射，用于回收
    std::map<void*, Span*> spanMap_;
    std::mutex mutex_;

};
