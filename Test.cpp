#include "MemoryPool.h"


#ifdef WIN32
#include <windows.h>
#endif

#include <assert.h>
#include <memory>
#include <random>
#include <iostream>
#include <chrono>

#if DEBUG
#   define TEST(x) assert(x)
#else
#   define TEST(x) x
#endif 

size_t g_pageSize;

bool Test_1()
{
    mem::MemoryPool<false, true> pool(g_pageSize);

    {
        void* a = pool.allocMemory(sizeof(int));
        assert(a != nullptr);
        int* c = reinterpret_cast<int*>(a);
        *c = 10;

        void* b = pool.allocMemory(sizeof(int));
        assert(b != nullptr);
        int* d = reinterpret_cast<int*>(b);
        *d = 11;
        assert(*(int*)a == 10);
        assert(*(int*)b == 11);
        std::cout << "After Create" << std::endl;
        pool.collectStatistic();

        pool.freeMemory(a);
        pool.freeMemory(b);
        std::cout << "After Destroy" << std::endl;
        pool.collectStatistic();
        assert(true);
    }

    {
        int* a = pool.allocElement<int>();
        assert(a != nullptr);
        *a = 10;
        assert(*a == 10);
        std::cout << "After Create" << std::endl;
        pool.collectStatistic();

        pool.freeMemory(a);
        std::cout << "After Destroy" << std::endl;
        pool.collectStatistic();
    }

    {
        struct A
        {
            char a;
            int b;
            double c;
        };

        A* a = pool.allocElement<A>();
        assert(a != nullptr);
        memset(a, 0, sizeof(A));
        std::cout << "After Create" << std::endl;
        pool.collectStatistic();

        pool.freeMemory(a);
        std::cout << "After Destroy" << std::endl;
        pool.collectStatistic();
    }

    {
        int count = 10;
        int* a = pool.allocArray<int>(count);
        for (int i = 0; i < count; ++i)
        {
            assert(&a[i] != nullptr);
        }
        std::cout << "After Create" << std::endl;
        pool.collectStatistic();

        pool.freeMemory(a);
        std::cout << "After Destroy" << std::endl;
        pool.collectStatistic();
    }
    std::cout << "After Destroy All" << std::endl;
    pool.collectStatistic();

    return true;
}

bool Test_2()
{
    mem::u64 allocateTime = 0;
    mem::u64 dealocateTime = 0;

    //large pools
    mem::MemoryPool<false, true> pool(g_pageSize);

    const int maxMallocSize = 10 * 1024 * 1024;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1024, maxMallocSize);

    std::vector<std::pair<int, void*>> sizesPool;
    std::vector<void*> sizesDefault;
 
    const int countAllocation = 1000;
    for (int i = 0; i < countAllocation; ++i)
    {
        int sz = dis(gen);
        sizesPool.push_back(std::make_pair(sz, nullptr));
    }

    for (auto& size : sizesPool)
    {
        size.second = pool.allocMemory(size.first);
        assert(size.second);
        memset(size.second, size.first, size.first);

        auto startTime = std::chrono::high_resolution_clock::now();
        void* ptr = malloc(size.first);
        auto endTime = std::chrono::high_resolution_clock::now();
        allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        assert(ptr);
        memset(ptr, size.first, size.first);
        sizesDefault.push_back(ptr);
    }
    std::cout << "After Create" << std::endl;
    pool.collectStatistic();

    for (int i = 0; i < sizesPool.size(); ++i)
    {
        int d = memcmp(sizesPool[i].second, sizesDefault[i], sizesPool[i].first);
        if (d != 0)
        {
            return false;
        }
    }

    std::mt19937 gen1(rd());
    std::shuffle(sizesPool.begin(), sizesPool.end(), gen1);

    for (auto& size : sizesPool)
    {
        pool.freeMemory(size.second);
    }
    std::cout << "After Destroy" << std::endl;
    pool.collectStatistic();

    for (auto& size : sizesDefault)
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        free(size);
        auto endTime = std::chrono::high_resolution_clock::now();
        dealocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    }

    std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)dealocateTime / 1000.0 << std::endl;

    return true;
}

bool Test_3()
{
    //small allocation
    /*mem::MemoryPool pool({ g_pageSize, true, false });

    const int maxMallocSize = 1024;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, maxMallocSize);

    std::vector<std::pair<int, void*>> sizesPool;
    std::vector<void*> sizesDefault;

    const int countAllocation = 1000;
    for (int i = 0; i < countAllocation; ++i)
    {
        int sz = dis(gen);
        sizesPool.push_back(std::make_pair(sz, nullptr));
    }

    for (auto& size : sizesPool)
    {
        size.second = pool.allocMemory(size.first);
        assert(size.second);
        memset(size.second, size.first, size.first);

        void* ptr = malloc(size.first);
        assert(ptr);
        memset(ptr, size.first, size.first);
        sizesDefault.push_back(ptr);
    }

    for (int i = 0; i < sizesPool.size(); ++i)
    {
        int d = memcmp(sizesPool[i].second, sizesDefault[i], sizesPool[i].first);
        if (d != 0)
        {
            return false;
        }
    }

    std::mt19937 gen1(rd());
    std::shuffle(sizesPool.begin(), sizesPool.end(), gen1);

    for (auto& size : sizesPool)
    {
        pool.freeMemory(size.second);
    }
    pool.collectStatistic();*/

    return true;
}

int main()
{
#ifdef WIN32
    SYSTEM_INFO systemInfo;
    memset(&systemInfo, 0, sizeof(SYSTEM_INFO));
    ::GetSystemInfo(&systemInfo);

    g_pageSize = systemInfo.dwPageSize;
#endif
    TEST(Test_1());
    TEST(Test_2());
    TEST(Test_3());

    return 0;
}