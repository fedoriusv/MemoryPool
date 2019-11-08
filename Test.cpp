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

bool Test_0()
{
    std::cout << "-----------------Test_0 (Small Allocation. Compare)-----------------" << std::endl;

    //create seq 32k allcation increase size, after that randomly delete it
    std::vector<std::pair<void*, size_t>> mem(32'768);
    {
        mem::MemoryPool pool(g_pageSize);
        pool.preAllocatePools();
        pool.collectStatistic();

        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < mem.size(); ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* ptr = pool.allocMemory(i + 1);
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr != nullptr);

            mem[i] = { ptr, i };
        }
        pool.collectStatistic();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(mem.begin(), mem.end(), gen);

        for (size_t i = 0; i < mem.size(); ++i)
        {
            auto& val = mem[i];
            assert(val.first != nullptr);
            auto startTime1 = std::chrono::high_resolution_clock::now();
            pool.freeMemory(val.first);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }
        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < mem.size(); ++i)
        {
            auto startTime = std::chrono::high_resolution_clock::now();
            void* ptr = malloc(i + 1);
            auto endTime = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            assert(ptr != nullptr);

            mem[i] = { ptr, i };
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(mem.begin(), mem.end(), gen);

        for (size_t i = 0; i < mem.size(); ++i)
        {
            assert(mem[i].first != nullptr);
            auto startTime = std::chrono::high_resolution_clock::now();
            free(mem[i].first);
            auto endTime = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_0 END-----------------" << std::endl;
    return true;
}

bool Test_1()
{
    std::cout << "----------------Test_1 (Small Allocation. Base)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize);

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

    std::cout << "----------------Test_1 END-----------------" << std::endl;
    return true;
}

bool Test_2()
{
    std::cout << "----------------Test_2 (Small Allocation. 2 alloc many times)-----------------" << std::endl;

    const int countIter = 100'000;
    {
        mem::MemoryPool pool(g_pageSize);
        pool.preAllocatePools();

        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < countIter; ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* ptr1 = pool.allocMemory(sizeof(int));
            void* ptr2 = pool.allocMemory(sizeof(int));
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);

            auto startTime1 = std::chrono::high_resolution_clock::now();
            pool.freeMemory(ptr2);
            pool.freeMemory(ptr1);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }
        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < countIter; ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* ptr1 = malloc(sizeof(int));
            void* ptr2 = malloc(sizeof(int));
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);

            auto startTime1 = std::chrono::high_resolution_clock::now();
            free(ptr1);
            free(ptr2);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_2 END-----------------" << std::endl;
    return true;
}

bool Test_3()
{
    std::cout << "----------------Test_3 (Small Allocation. Check content)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize);

    for (size_t i = 0; i < 2; ++i)
    {
        size_t koef = (i * 10) + 1;
        {
            void* a = pool.allocMemory(30 * koef);
            assert(a != nullptr);
            memset(a, 'a', 30 * koef);

            void* b = pool.allocMemory(40 * koef);
            assert(b != nullptr);
            memset(b, 'b', 40 * koef);

            void* c = pool.allocMemory(50 * koef);
            assert(c != nullptr);
            memset(c, 'c', 50 * koef);

            void* d = pool.allocMemory(60 * koef);
            assert(d != nullptr);
            memset(d, 'd', 60 * koef);

            {
                char* ca = (char*)a;
                for (int i = 0; i < 30 * koef; ++i)
                {
                    if (ca[i] != 'a')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* cb = (char*)b;
                for (int i = 0; i < 40 * koef; ++i)
                {
                    if (cb[i] != 'b')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* cc = (char*)c;
                for (int i = 0; i < 50 * koef; ++i)
                {
                    if (cc[i] != 'c')
                    {
                        assert(false);
                        return false;
                    }
                }
            }

            char* cd = (char*)d;
            for (int i = 0; i < 60 * koef; ++i)
            {
                if (cd[i] != 'd')
                {
                    assert(false);
                    return false;
                }
            }

            pool.freeMemory(b);
            b = nullptr;
            pool.freeMemory(c);
            c = nullptr;

            void* e = pool.allocMemory(80 * koef);
            assert(e != nullptr);
            memset(e, 'e', 80 * koef);

            void* f = pool.allocMemory(80 * koef);
            assert(f != nullptr);
            memset(f, 'f', 80 * koef);

            {
                char* cd = (char*)d;
                for (int i = 0; i < 60 * koef; ++i)
                {
                    if (cd[i] != 'd')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* ce = (char*)e;
                for (int i = 0; i < 80 * koef; ++i)
                {
                    if (ce[i] != 'e')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* cf = (char*)f;
                for (int i = 0; i < 80 * koef; ++i)
                {
                    if (cf[i] != 'f')
                    {
                        assert(false);
                        return false;
                    }
                }
            }

            std::cout << "After Create" << std::endl;
            pool.collectStatistic();
        }
    }
        std::cout << "After Destroy" << std::endl;
        pool.collectStatistic();
        assert(true);

    std::cout << "After Destroy All" << std::endl;
    pool.collectStatistic();

    std::cout << "----------------Test_3 END-----------------" << std::endl;
    return true;
}

bool Test_4()
{
    std::cout << "----------------Test_4 (Medium allocation)-----------------" << std::endl;

    //create 32k-64k rendom allcation increase size, after that randomly delete it
    mem::MemoryPool pool(g_pageSize);

    const size_t minMallocSize = 32'736;
    const size_t maxMallocSize = g_pageSize - 1;  //64 KB

    const size_t testSize = 32'000;
    std::vector<std::pair<void*, int>> mem(testSize);
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(minMallocSize, maxMallocSize);

        for (int i = 0; i < mem.size(); ++i)
        {
            int sz = dis(gen);
            mem[i] = std::make_pair(nullptr, sz);
        }
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < mem.size(); ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* ptr = pool.allocMemory(mem[i].second);
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr != nullptr);
            mem[i].first = ptr;
        }
        pool.collectStatistic();

        std::vector<std::pair<void*, int>> memSuffle(testSize);
        std::copy(mem.begin(), mem.end(), memSuffle.begin());
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(memSuffle.begin(), memSuffle.end(), gen);

        for (size_t i = 0; i < memSuffle.size(); ++i)
        {
            auto& val = memSuffle[i];
            assert(val.first != nullptr);
            auto startTime1 = std::chrono::high_resolution_clock::now();
            pool.freeMemory(val.first);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }
        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < mem.size(); ++i)
        {
            auto startTime = std::chrono::high_resolution_clock::now();
            void* ptr = malloc(mem[i].second);
            auto endTime = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            assert(ptr != nullptr);
            mem[i].first = ptr;
        }

        std::vector<std::pair<void*, int>> memSuffle(testSize);
        std::copy(mem.begin(), mem.end(), memSuffle.begin());
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(memSuffle.begin(), memSuffle.end(), gen);

        for (size_t i = 0; i < memSuffle.size(); ++i)
        {
            assert(mem[i].first != nullptr);
            auto startTime = std::chrono::high_resolution_clock::now();
            free(memSuffle[i].first);
            auto endTime = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_4 END-----------------" << std::endl;
    return true;
}

bool Test_5()
{
    std::cout << "----------------Test_5 (Medium allocation)-----------------" << std::endl;
    std::cout << "----------------Test_5 END-----------------" << std::endl;
    return true;
}

bool Test_6()
{
    std::cout << "----------------Test_6 (Medium allocation)-----------------" << std::endl;
    std::cout << "----------------Test_6 END-----------------" << std::endl;
    return true;
}

bool Test_7()
{
    std::cout << "----------------Test_7 (Large allocation)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize);
    const size_t minMallocSize = g_pageSize;
    const size_t maxMallocSize = 1024 * 1024 * 512;  //512 MB

    std::vector<std::pair<void*, int>> mem(10);
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(minMallocSize, maxMallocSize);

        for (size_t i = 0; i < mem.size(); ++i)
        {
            int sz = dis(gen);
            void* ptr = pool.allocMemory(sz);
            mem[i] = std::make_pair(ptr, sz);
            assert(mem[i].first != nullptr);
        }
        pool.collectStatistic();
    }

    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(mem.begin(), mem.end(), gen);

        for (size_t i = 0; i < mem.size(); ++i)
        {
            assert(mem[i].first != nullptr);
            pool.freeMemory(mem[i].first);
        }
        pool.collectStatistic();
    }
    std::cout << "----------------Test_7 END-----------------" << std::endl;
    return true;
}


bool Test_8()
{
    std::cout << "----------------Test_8 (Combine allocation)-----------------" << std::endl;

    mem::u64 allocateTime = 0;
    mem::u64 deallocateTime = 0;

    //large pools
    mem::MemoryPool pool(g_pageSize);

    const int maxMallocSize = 10 * 1024 * 1024;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1024, maxMallocSize);

    std::vector<std::pair<int, void*>> sizesPool;
    std::vector<void*> sizesDefault;
 
    const int countAllocation = 1000;
    for (size_t i = 0; i < countAllocation; ++i)
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

    for (size_t i = 0; i < sizesPool.size(); ++i)
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
        deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    }

    std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;

    std::cout << "----------------Test_8 END-----------------" << std::endl;
    return true;
}

bool Test_9()
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

    g_pageSize = systemInfo.dwAllocationGranularity;
#endif
    TEST(Test_0());
    TEST(Test_1());
    TEST(Test_2());
    TEST(Test_3());
   /* TEST(Test_4());
    TEST(Test_5());
    TEST(Test_6());*/
    TEST(Test_7());
    //TEST(Test_8());

    return 0;
}