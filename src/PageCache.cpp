#include "PageCache.h"
#include "Common.h"
#include <sys/mman.h>
#include <cstring>

void* PageCache::allocateSpan(size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找合适的空闲span
    // lower_bound函数返回第一个大于等于numPages的元素的迭代器
    //可能你要4页的span，如果有4页的，就返回指向4页的span的迭代器，否则就可能指向5页的span迭代器
    auto it = freeSpans_.lower_bound(numPages);

    //如果有空闲的span可以分配
    if (it != freeSpans_.end())
    {
        //把头拿出来
        Span* span = it->second;

        // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
        //如果span下面还有下一个该页数的span，那下一个变成头，我取第一个走
        if (span->next)
        {
            freeSpans_[it->first] = span->next;
        }
        else
        {
            //如果只有这一个结点，那这条链就该移除了
            freeSpans_.erase(it);
        }

        // 如果span大于需要的numPages则进行分割
        if (span->numPages > numPages) 
        {
            //这个newspan就是要切走的页
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + 
                                numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            // 好引用，将超出部分放回空闲Span*列表头部
            auto& list = freeSpans_[newSpan->numPages];
            //插到头上
            newSpan->next = list;
            //变成头
            list = newSpan;

            //spanMap_[span->pageAddr] = span;
            spanMap_[newSpan->pageAddr] = newSpan;
            span->numPages = numPages;
        }

        // 记录span信息用于回收
        spanMap_[span->pageAddr] = span;
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请，只申请刚好够numPages页
    void* memory = systemAlloc(numPages);
    if (!memory) return nullptr;

    // 创建新的span
    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    // 记录span信息用于回收
    spanMap_[memory] = span;
    return memory;
}


void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;

    // 使用mmap分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;

    // 清零内存
    //memset(ptr, 0, size);
    return ptr;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) return;

    Span* span = it->second;

    // 从空闲链表中摘掉指定 span；成功返回 true，失败(不在空闲)返回 false
    auto removeFromFreeList = [&](Span* s) -> bool {
        auto listIt = freeSpans_.find(s->numPages);
        if (listIt == freeSpans_.end()) return false; // 没这条链，肯定不在空闲
        Span*& head = listIt->second;
        if (!head) return false;

        if (head == s) {                 // 头结点
            head = s->next;
            s->next = nullptr;           // 清理 next，避免脏指针
            return true;
        }
        for (Span* p = head; p->next; p = p->next) { // 中间/尾结点
            if (p->next == s) {
                p->next = s->next;
                s->next = nullptr;       // 清理 next
                return true;
            }
        }
        return false;                    // 不在空闲链（说明正在被使用）
    };

    // 反复尝试向后、向前合并，直到不能再合并
    bool merged;
    do {
        merged = false;

        // ===== 向后合并：span | nextSpan  ->  span =====
        {
            void* nextAddr = static_cast<char*>(span->pageAddr) + span->numPages * PAGE_SIZE;
            auto nextIt = spanMap_.find(nextAddr);
            if (nextIt != spanMap_.end()) {
                Span* nextSpan = nextIt->second;

                // 只和“空闲”的后邻合并
                if (removeFromFreeList(nextSpan)) {
                    span->numPages += nextSpan->numPages;  // 扩大当前 span
                    spanMap_.erase(nextIt);                // 移除后邻 begin 映射
                    delete nextSpan;                       // 释放被吸收的元数据
                    merged = true;
                }
            }
        }

        // ===== 向前合并：prevSpan | span  ->  prevSpan =====
        {
            auto currIt = spanMap_.find(span->pageAddr);
            if (currIt != spanMap_.begin()) {
                auto prevIt = std::prev(currIt);
                Span* prev = prevIt->second;

                char* prevEnd = static_cast<char*>(prev->pageAddr) + prev->numPages * PAGE_SIZE;
                if (prevEnd == span->pageAddr) {
                    // 只和“空闲”的前邻合并
                    if (removeFromFreeList(prev)) {
                        Span* cur = span;                  // 当前 span 被吸收
                        prev->numPages += cur->numPages;   // 扩大前邻
                        spanMap_.erase(cur->pageAddr);     // 移除当前 begin 映射
                        delete cur;                        // 释放被吸收的元数据
                        span = prev;                       // 锚点改为合并后的前邻
                        merged = true;
                    }
                }
            }
        }

    } while (merged);

    // 合并完成后，把大 span 头插到对应页数的空闲链
    span->next = freeSpans_[span->numPages];
    freeSpans_[span->numPages] = span;
}


PageCache::~PageCache() {
        shutdown();
}

void PageCache::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : spanMap_) {
        delete kv.second;
    }

    spanMap_.clear();

    // freeSpans_ 只是引用 span，不要重复 delete；清指针即可
    freeSpans_.clear();
}
