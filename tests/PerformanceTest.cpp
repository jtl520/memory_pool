#include "MemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>
#include "PageCache.h"
#include "mymemory.h"

using namespace std::chrono;
#define nomy 0

// 计时器类
class Timer 
{
    high_resolution_clock::time_point start;
public:
    Timer() : start(high_resolution_clock::now()) {}
    
    double elapsed() 
    {
        auto end = high_resolution_clock::now();
        return duration_cast<microseconds>(end - start).count() / 1000.0; // 转换为毫秒
    }
};

// 性能测试类
class PerformanceTest 
{
private:
    // 测试统计信息
    struct TestStats 
    {
        double memPoolTime{0.0};
        double systemTime{0.0};
        size_t totalAllocs{0};
        size_t totalBytes{0};
    };

public:
    // 1. 系统预热
    static void warmup() 
    {
        std::cout << "Warming up memory systems...\n";
        // 使用 pair 来存储指针和对应的大小
        std::vector<std::pair<void*, size_t>> warmupPtrs;
        
        // 预热内存池
        for (int i = 0; i < 1000; ++i) 
        {
            for (size_t size : {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536}) {
#if nomy
                void* p = MemoryPool::allocate(size);  // 使用实际分配的大小进行释放
#else
                void* p =CMemory::GetInstance()->AllocMemory(size,false);  // 使用实际分配的大小进行释放
#endif
                warmupPtrs.emplace_back(p, size);  // 存储指针和对应的大小
            }
        }
        
        // 释放预热内存
        for (const auto& [ptr, size] : warmupPtrs) 
        {
#if nomy
            MemoryPool::deallocate(ptr, size);  // 使用实际分配的大小进行释放
#else
            CMemory::GetInstance()->FreeMemory(ptr);  // 使用实际分配的大小进行释放
#endif
        }
        
        std::cout << "Warmup complete.\n\n";
    }

    // 2. 小对象分配测试
    static void testSmallAllocation() 
    {
        constexpr size_t NUM_ALLOCS = 100000;
        constexpr size_t SMALL_SIZE = 32;
        
        std::cout << "\nTesting small allocations (" << NUM_ALLOCS << " allocations of " 
                  << SMALL_SIZE << " bytes):" << std::endl;
        
        // 测试内存池
        {
            Timer t;
            std::vector<void*> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
#if nomy
                void* p = MemoryPool::allocate(SMALL_SIZE);  // 使用实际分配的大小进行释放
#else
                void* p =CMemory::GetInstance()->AllocMemory(SMALL_SIZE,false);  // 使用实际分配的大小进行释放
#endif
                ptrs.push_back(p);
                
                // 模拟真实使用：部分立即释放
                if (i % 4 == 0) 
                {
#if nomy
                    MemoryPool::deallocate(ptrs.back(), SMALL_SIZE);
#else
                    CMemory::GetInstance()->FreeMemory(ptrs.back());
#endif
                    ptrs.pop_back();
                }
            }

            //然后全部释放
            for (void* ptr : ptrs) 
            {
#if nomy
                MemoryPool::deallocate(ptr, SMALL_SIZE);
#else
                CMemory::GetInstance()->FreeMemory(ptr);
#endif
            }
            
            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
        
        // 测试new/delete
        {
            Timer t;
            std::vector<void*> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                ptrs.push_back(new char[SMALL_SIZE]);
                
                if (i % 4 == 0) 
                {
                    delete[] static_cast<char*>(ptrs.back());
                    ptrs.pop_back();
                }
            }
            
            for (void* ptr : ptrs) 
            {
                delete[] static_cast<char*>(ptr);
            }
            
            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
    }
    
    // 3. 多线程测试
    static void testMultiThreaded() 
    {
        constexpr size_t NUM_THREADS = 4;
        //constexpr size_t ALLOCS_PER_THREAD = 25000;
        constexpr size_t ALLOCS_PER_THREAD = 25000;
        //constexpr size_t MAX_SIZE = 256;
        constexpr size_t MAX_SIZE = 4 * 1024;

        std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS 
                  << " threads, " << ALLOCS_PER_THREAD << " allocations each):" 
                  << std::endl;
        
        auto threadFunc = [](bool useMemPool) 
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(8, MAX_SIZE);
            std::vector<std::pair<void*, size_t>> ptrs;
            ptrs.reserve(ALLOCS_PER_THREAD);
            
            for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i) 
            {
                size_t size = dis(gen);
#if nomy
                void* ptr = useMemPool ? MemoryPool::allocate(size)
                                     : new char[size];  // 使用实际分配的大小进行释放
#else
                void* ptr = useMemPool ? CMemory::GetInstance()->AllocMemory(size,false)
                                     : new char[size];  // 使用实际分配的大小进行释放
#endif
                ptrs.push_back({ptr, size});
                
                // 随机释放一些内存
                if (rand() % 100 < 75) 
                {  // 75%的概率释放
                    size_t index = rand() % ptrs.size();
                    if (useMemPool) {
#if nomy
                        MemoryPool::deallocate(ptrs[index].first, ptrs[index].second);
#else
                        CMemory::GetInstance()->FreeMemory(ptrs[index].first);
#endif
                    } else {
                        delete[] static_cast<char*>(ptrs[index].first);
                    }
                    ptrs[index] = ptrs.back();
                    ptrs.pop_back();
                }
            }
            
            // 清理剩余内存
            for (const auto& [ptr, size] : ptrs) 
            {
                if (useMemPool) 
                {
#if nomy
                    MemoryPool::deallocate(ptr, size);
#else
                    CMemory::GetInstance()->FreeMemory(ptr);
#endif
                }
                else 
                {
                    delete[] static_cast<char*>(ptr);
                }
            }
        };
        
        // 测试内存池
        {
            Timer t;
            std::vector<std::thread> threads;
            
            for (size_t i = 0; i < NUM_THREADS; ++i) 
            {
                threads.emplace_back(threadFunc, true);
            }
            
            for (auto& thread : threads) 
            {
                thread.join();
            }
            
            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
        
        // 测试new/delete
        {
            Timer t;
            std::vector<std::thread> threads;
            
            for (size_t i = 0; i < NUM_THREADS; ++i) 
            {
                threads.emplace_back(threadFunc, false);
            }
            
            for (auto& thread : threads) 
            {
                thread.join();
            }
            
            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
    }
    
    // 4. 混合大小测试
    static void testMixedSizes() 
    {
        //constexpr size_t NUM_ALLOCS = 50000;
        constexpr size_t NUM_ALLOCS = 50000;
        //const size_t SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
        const size_t SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

        std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS 
                  << " allocations):" << std::endl;
        
        // 测试内存池
        {
            Timer t;
            std::vector<std::pair<void*, size_t>> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                //size_t size = SIZES[rand() % 8];
                size_t size = SIZES[rand() % 13];
#if nomy
                void* p = MemoryPool::allocate(size);
#else
                void* p = CMemory::GetInstance()->AllocMemory(size,false);
#endif
                ptrs.emplace_back(p, size);
                
                // 批量释放
                if (i % 100 == 0 && !ptrs.empty()) 
                {
                    size_t releaseCount = std::min(ptrs.size(), size_t(20));
                    for (size_t j = 0; j < releaseCount; ++j) 
                    {
#if nomy
                        MemoryPool::deallocate(ptrs.back().first, ptrs.back().second);
#else
                        CMemory::GetInstance()->FreeMemory(ptrs.back().first);
#endif
                        ptrs.pop_back();
                    }
                }
            }
            
            for (const auto& [ptr, size] : ptrs) 
            {
#if nomy
                MemoryPool::deallocate(ptr, size);
#else
                CMemory::GetInstance()->FreeMemory(ptr);
#endif
            }
            
            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
        
        // 测试new/delete
        {
            Timer t;
            std::vector<std::pair<void*, size_t>> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                //size_t size = SIZES[rand() % 8];
                size_t size = SIZES[rand() % 13];
                void* p = new char[size];
                ptrs.emplace_back(p, size);
                
                if (i % 100 == 0 && !ptrs.empty()) 
                {
                    size_t releaseCount = std::min(ptrs.size(), size_t(20));
                    for (size_t j = 0; j < releaseCount; ++j) 
                    {
                        delete[] static_cast<char*>(ptrs.back().first);
                        ptrs.pop_back();
                    }
                }
            }
            
            for (const auto& [ptr, size] : ptrs) 
            {
                delete[] static_cast<char*>(ptr);
            }
            
            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
    }
};

int main() 
{
    std::cout << "Starting performance tests..." << std::endl;
    std::cout << std::endl;
    
    // 预热系统
    PerformanceTest::warmup();
    
    // 运行测试
    PerformanceTest::testSmallAllocation();
    PerformanceTest::testMultiThreaded();
    PerformanceTest::testMixedSizes();

    PageCache::getInstance().shutdown();  // 显式清理

    // 越界写
    // char* p = (char*)malloc(8);
    // p[8] = 1;  // OOB
    // // 故意泄漏
    // (void)new int[10];
    return 0;
}