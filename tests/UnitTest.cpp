#include "MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include <cstring>
#include <random>
#include <algorithm>
#include <atomic>
#include <PageCache.h>
#include "mymemory.h"

#define nomy 0

// 基础分配测试
void testBasicAllocation() 
{
    std::cout << "Running basic allocation test..." << std::endl;
    std::cout<<std::endl;
    
    // 测试小内存分配
#if nomy
    void* ptr1 = MemoryPool::allocate(8);  // 使用实际分配的大小进行释放
#else
    void* ptr1 =CMemory::GetInstance()->AllocMemory(8,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr1 != nullptr);

#if nomy
    MemoryPool::deallocate(ptr1, 8);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr1);  // 使用实际分配的大小进行释放
#endif

    // 测试中等大小内存分配
#if nomy
    void* ptr2 = MemoryPool::allocate(1024);  // 使用实际分配的大小进行释放
#else
    void* ptr2 =CMemory::GetInstance()->AllocMemory(1024,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr2 != nullptr);

#if nomy
    MemoryPool::deallocate(ptr2, 1024);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr2);  // 使用实际分配的大小进行释放
#endif

    // 测试大内存分配（超过MAX_BYTES）
#if nomy
    void* ptr3 = MemoryPool::allocate(1024 * 1024);  // 使用实际分配的大小进行释放
#else
    void* ptr3 =CMemory::GetInstance()->AllocMemory(1024 * 1024,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr3 != nullptr);

#if nomy
    MemoryPool::deallocate(ptr3, 1024 * 1024);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr3);  // 使用实际分配的大小进行释放
#endif

    std::cout << "Basic allocation test passed!" << std::endl;
    std::cout<<std::endl;
}

// 内存写入测试
void testMemoryWriting() 
{
    std::cout << "Running memory writing test..." << std::endl;
    std::cout<<std::endl;

    // 分配并写入数据
    const size_t size = 128;

#if nomy
    void* ptr1 = MemoryPool::allocate(size);  // 使用实际分配的大小进行释放
#else
    void* ptr1 =CMemory::GetInstance()->AllocMemory(size,false);  // 使用实际分配的大小进行释放
#endif

    char* ptr = static_cast<char*>(ptr1);
    assert(ptr != nullptr);

    // 写入数据
    for (size_t i = 0; i < size; ++i) 
    {
        ptr[i] = static_cast<char>(i % 256);
    }

    // 验证数据
    for (size_t i = 0; i < size; ++i) 
    {
        assert(ptr[i] == static_cast<char>(i % 256));
    }

#if nomy
    MemoryPool::deallocate(ptr, size);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr);  // 使用实际分配的大小进行释放
#endif

    std::cout << "Memory writing test passed!" << std::endl;
    std::cout<<std::endl;
}

// 多线程测试
void testMultiThreading() 
{
    std::cout << "Running multi-threading test..." << std::endl;
    std::cout<<std::endl;

    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 1000;
    std::atomic<bool> has_error{false};
    
    auto threadFunc = [&has_error]() 
    {
        try 
        {
            std::vector<std::pair<void*, size_t>> allocations;
            allocations.reserve(ALLOCS_PER_THREAD);
            
            for (int i = 0; i < ALLOCS_PER_THREAD && !has_error; ++i) 
            {
                //0-2k
                size_t size = (rand() % 256 + 1) * 8;
#if nomy
                void* ptr = MemoryPool::allocate(size);  // 使用实际分配的大小进行释放
#else
                void* ptr =CMemory::GetInstance()->AllocMemory(size,false);  // 使用实际分配的大小进行释放
#endif
                
                if (!ptr) 
                {
                    std::cerr << "Allocation failed for size: " << size << std::endl;
                    has_error = true;
                    break;
                }
                
                allocations.push_back({ptr, size});

                //随机释放某些内存
                if (rand() % 2 && !allocations.empty()) 
                {
                    size_t index = rand() % allocations.size();
#if nomy
                    MemoryPool::deallocate(allocations[index].first,
                                         allocations[index].second); // 使用实际分配的大小进行释放
#else
                    CMemory::GetInstance()->FreeMemory(allocations[index].first);  // 使用实际分配的大小进行释放
#endif
                    allocations.erase(allocations.begin() + index);
                }
            }

            //释放所有内存
            for (const auto& alloc : allocations) 
            {
#if nomy
                MemoryPool::deallocate(alloc.first,
                                     alloc.second); // 使用实际分配的大小进行释放
#else
                CMemory::GetInstance()->FreeMemory(alloc.first);  // 使用实际分配的大小进行释放
#endif
            }
        }
        catch (const std::exception& e) 
        {
            std::cerr << "Thread exception: " << e.what() << std::endl;
            has_error = true;
        }
    };

    std::vector<std::thread> threads;

    //4个线程，每个分配1000次，每次 0-2k
    for (int i = 0; i < NUM_THREADS; ++i) 
    {
        threads.emplace_back(threadFunc);
    }

    for (auto& thread : threads) 
    {
        thread.join();
    }

    std::cout << "Multi-threading test passed!" << std::endl;
    std::cout<<std::endl;
}

// 边界测试
void testEdgeCases() 
{
    std::cout << "Running edge cases test..." << std::endl;
    std::cout<<std::endl;
    
    // 测试0大小分配

#if nomy
    void* ptr1 = MemoryPool::allocate(0);  // 使用实际分配的大小进行释放
#else
    void* ptr1 =CMemory::GetInstance()->AllocMemory(0,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr1 != nullptr);

#if nomy
    MemoryPool::deallocate(ptr1, 0);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr1);  // 使用实际分配的大小进行释放
#endif
    
    // 测试最小对齐大小
#if nomy
    void* ptr2 = MemoryPool::allocate(1);  // 使用实际分配的大小进行释放
#else
    void* ptr2 =CMemory::GetInstance()->AllocMemory(1,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr2 != nullptr);
    assert((reinterpret_cast<uintptr_t>(ptr2) & (ALIGNMENT - 1)) == 0);

#if nomy
    MemoryPool::deallocate(ptr2, 1);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr2);  // 使用实际分配的大小进行释放
#endif
    
    // 测试最大大小边界
#if nomy
    void* ptr3 = MemoryPool::allocate(MAX_BYTES);  // 使用实际分配的大小进行释放
#else
    void* ptr3 =CMemory::GetInstance()->AllocMemory(MAX_BYTES,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr3 != nullptr);

#if nomy
    MemoryPool::deallocate(ptr3, MAX_BYTES);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr3);  // 使用实际分配的大小进行释放
#endif
    
    // 测试超过最大大小
#if nomy
    void* ptr4 = MemoryPool::allocate(MAX_BYTES + 1);  // 使用实际分配的大小进行释放
#else
    void* ptr4 =CMemory::GetInstance()->AllocMemory(MAX_BYTES + 1,false);  // 使用实际分配的大小进行释放
#endif

    assert(ptr4 != nullptr);

#if nomy
    MemoryPool::deallocate(ptr4, MAX_BYTES + 1);  // 使用实际分配的大小进行释放
#else
    CMemory::GetInstance()->FreeMemory(ptr4);  // 使用实际分配的大小进行释放
#endif
    
    std::cout << "Edge cases test passed!" << std::endl;
    std::cout<<std::endl;
}

// 压力测试
void testStress() 
{
    std::cout << "Running stress test..." << std::endl;
    std::cout<<std::endl;

    const int NUM_ITERATIONS = 10000;
    std::vector<std::pair<void*, size_t>> allocations;
    allocations.reserve(NUM_ITERATIONS);

    for (int i = 0; i < NUM_ITERATIONS; ++i) 
    {
        //0-8k,10000次
        size_t size = (rand() % 1024 + 1) * 8;
#if nomy
        void* ptr = MemoryPool::allocate(size);  // 使用实际分配的大小进行释放
#else
        void* ptr =CMemory::GetInstance()->AllocMemory(size,false);  // 使用实际分配的大小进行释放
#endif

        assert(ptr != nullptr);

        allocations.push_back({ptr, size});
    }

    // 随机顺序释放
    std::random_device rd;
    std::mt19937 g(rd());
    //打乱容器内元素顺序
    std::shuffle(allocations.begin(), allocations.end(), g);
    for (const auto& alloc : allocations) 
    {
#if nomy
        MemoryPool::deallocate(alloc.first,
                             alloc.second); // 使用实际分配的大小进行释放
#else
        CMemory::GetInstance()->FreeMemory(alloc.first);  // 使用实际分配的大小进行释放
#endif
    }

    std::cout << "Stress test passed!" << std::endl;
    std::cout<<std::endl;
}

int main() 
{
    try 
    {
        std::cout << "Starting memory pool tests..." << std::endl;
        std::cout<<std::endl;

        testBasicAllocation();
        testMemoryWriting();
        testMultiThreading();
        testEdgeCases();
        testStress();

        std::cout << "All tests passed successfully!" << std::endl;
        std::cout<<std::endl;
        return 0;
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        PageCache::getInstance().shutdown();
        return 1;
    }

}