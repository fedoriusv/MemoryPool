#include "MemoryPool.h"

#include <assert.h>
#include <memory>
#include <random>
#include <iostream>
#include <chrono>
#include <type_traits>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include "android_native_app_glue.h"
#include "AndroidLogger.hpp"
#endif //__ANDROID__

#if DEBUG
#   define TEST(x) assert(x)
#else
#   define TEST(x) x
#endif

#ifdef WIN32
class WinMemoryAllocator : public mem::MemoryPool::MemoryAllocator
{
public:

    explicit WinMemoryAllocator() noexcept
    {
        s_heap = HeapCreate(0, 0, 0);
    }
    ~WinMemoryAllocator() = default;

    mem::address_ptr allocate(mem::u64 size, mem::u32 aligment = 0, void* user = nullptr) override
    {
        LPVOID ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        //LPVOID ptr = HeapAlloc(s_heap, 0, size);
        assert(ptr);
        return ptr;

        //return new char[size];
    }

    void deallocate(mem::address_ptr memory, mem::u64 size = 0, void* user = nullptr) override
    {
        //BOOL result = HeapFree(s_heap, 0, memory);
        BOOL result = VirtualFree(memory, 0, MEM_RELEASE);
        assert(result);

        //delete[] memory;
    }

    static inline HANDLE s_heap = nullptr;
};
#endif //WIN32

#ifdef __ANDROID__
class AndroidMemoryAllocator : public mem::MemoryPool::MemoryAllocator
{
public:

    explicit AndroidMemoryAllocator() noexcept = default;
    ~AndroidMemoryAllocator() = default;

    mem::address_ptr allocate(mem::u64 size, mem::u32 aligment = 0, void* user = nullptr) override
    {
        mem::address_ptr ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        assert(ptr);
        return ptr;
    }

    void deallocate(mem::address_ptr memory, mem::u64 size = 0, void* user = nullptr) override
    {
        int result = munmap(memory, size);
        assert(result == 0);
    }
};
#endif //__ANDROID__

size_t g_pageSize;

#ifdef WIN32
WinMemoryAllocator g_allocator;
//mem::DefaultMemoryAllocator g_allocator;
#elif __ANDROID__
AndroidMemoryAllocator g_allocator;
//mem::DefaultMemoryAllocator g_allocator;
#else
mem::DefaultMemoryAllocator g_allocator;
#endif

bool Test_0()
{
    std::cout << "----------------Test_0 (Small Allocation. Base)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize, &g_allocator);

    {
        void* volatile a = pool.allocMemory(sizeof(int));
        assert(a != nullptr);
        int* c = reinterpret_cast<int*>(a);
        *c = 10;

        void* volatile b = pool.allocMemory(sizeof(int));
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

    std::cout << "----------------Test_0 END-----------------" << std::endl;
    return true;
}

bool Test_1()
{
    std::cout << "-----------------Test_1 (Small Allocation. Compare)-----------------" << std::endl;

    //create seq 32k allcation increase size, after that randomly delete it
    std::vector<std::pair<void* volatile, size_t>> mem(32'768);
    {
        mem::MemoryPool pool(g_pageSize, &g_allocator);
        pool.preAllocatePools();
        pool.collectStatistic();

        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t j = 0; j < 2; ++j)
        {
            for (size_t i = 0; i < mem.size(); ++i)
            {
                auto startTime0 = std::chrono::high_resolution_clock::now();
                void* volatile ptr = pool.allocMemory(i + 1);
                auto endTime0 = std::chrono::high_resolution_clock::now();
                allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

                assert(ptr != nullptr);

                mem[i] = { ptr, i };
                memset(ptr, (int)i, i + 1);
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
        }
        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t j = 0; j < 2; ++j)
        {
            for (size_t i = 0; i < mem.size(); ++i)
            {
                auto startTime = std::chrono::high_resolution_clock::now();
                void* volatile ptr = malloc(i + 1);
                auto endTime = std::chrono::high_resolution_clock::now();
                allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                assert(ptr != nullptr);

                mem[i] = { ptr, i };
                memset(ptr, (int)i, i + 1);
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
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_1 END-----------------" << std::endl;
    return true;
}

bool Test_2()
{
    std::cout << "----------------Test_2 (Small Allocation. 2 alloc many times)-----------------" << std::endl;

    const size_t countIter = 10'000'00;
    {
        mem::MemoryPool pool(g_pageSize, &g_allocator);
        pool.preAllocatePools();

        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < countIter; ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* volatile ptr1 = pool.allocMemory(sizeof(int));
            void* volatile ptr2 = pool.allocMemory(sizeof(int));
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);
            memset(ptr1, (int)i, sizeof(int));
            memset(ptr2, (int)i, sizeof(int));

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
            void* volatile ptr1 = malloc(sizeof(int));
            void* volatile ptr2 = malloc(sizeof(int));
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);
            memset(ptr1, (int)i, sizeof(int));
            memset(ptr2, (int)i, sizeof(int));

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

    mem::MemoryPool pool(g_pageSize, &g_allocator);

    void* volatile ga[2];
    void* volatile gb[2];
    void* volatile gc[2];
    void* volatile gd[2];
    void* volatile ge[2];
    void* volatile gf[2];

    for (size_t i = 0; i < 2; ++i)
    {
        size_t koef = (i * 10) + 1;
        {
            void* volatile a = pool.allocMemory(30 * koef);
            assert(a != nullptr);
            memset(a, 'a', 30 * koef);
            ga[i] = a;

            void* volatile b = pool.allocMemory(40 * koef);
            assert(b != nullptr);
            memset(b, 'b', 40 * koef);
            gb[i] = b;

            void* volatile c = pool.allocMemory(50 * koef);
            assert(c != nullptr);
            memset(c, 'c', 50 * koef);
            gc[i] = c;

            void* volatile d = pool.allocMemory(60 * koef);
            assert(d != nullptr);
            memset(d, 'd', 60 * koef);
            gd[i] = d;

            {
                char* ca = (char*)a;
                for (size_t i = 0; i < 30 * koef; ++i)
                {
                    if (ca[i] != 'a')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* cb = (char*)b;
                for (size_t i = 0; i < 40 * koef; ++i)
                {
                    if (cb[i] != 'b')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* cc = (char*)c;
                for (size_t i = 0; i < 50 * koef; ++i)
                {
                    if (cc[i] != 'c')
                    {
                        assert(false);
                        return false;
                    }
                }
            }

            char* cd = (char*)d;
            for (size_t i = 0; i < 60 * koef; ++i)
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

            void* volatile e = pool.allocMemory(80 * koef);
            assert(e != nullptr);
            memset(e, 'e', 80 * koef);
            ge[i] = e;

            void* volatile f = pool.allocMemory(80 * koef);
            assert(f != nullptr);
            memset(f, 'f', 80 * koef);
            gf[i] = f;

            {
                char* cd = (char*)d;
                for (size_t i = 0; i < 60 * koef; ++i)
                {
                    if (cd[i] != 'd')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* ce = (char*)e;
                for (size_t i = 0; i < 80 * koef; ++i)
                {
                    if (ce[i] != 'e')
                    {
                        assert(false);
                        return false;
                    }
                }

                char* cf = (char*)f;
                for (size_t i = 0; i < 80 * koef; ++i)
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

        std::cout << "After Destroy" << std::endl;
        pool.collectStatistic();
        assert(true);
    }

    for (size_t i = 0; i < 2; ++i)
    {
        pool.freeMemory(ga[i]);
        //pool.freeMemory(gb); already deleted
        //pool.freeMemory(gc); already deleted
        pool.freeMemory(gd[i]);
        pool.freeMemory(ge[i]);
        pool.freeMemory(gf[i]);
    }

    std::cout << "After Destroy All" << std::endl;
    pool.collectStatistic();

    std::cout << "----------------Test_3 END-----------------" << std::endl;
    return true;
}

bool Test_4()
{
    std::cout << "----------------Test_4 (Medium allocation. Content test)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize, &g_allocator);

    size_t size = 50'000;
    assert(g_pageSize > size);
    const size_t countSize = 40;
    void* volatile ptrPool[countSize] = {};
    void* volatile ptrMalloc[countSize] = {};


    for (size_t i = 0; i < countSize; ++i)
    {
        ptrPool[i] = pool.allocMemory(size);
        assert(ptrPool[i] != nullptr);
        memset(ptrPool[i], (int)i, size);

        ptrMalloc[i] = malloc(size);
        assert(ptrMalloc[i] != nullptr);
        memset(ptrMalloc[i], (int)i, size);
    }
    pool.collectStatistic();

    for (size_t i = 0; i < countSize; ++i)
    {
        if (memcmp(ptrPool[i], ptrMalloc[i], size) != 0)
        {
            return false;
        }
    }

    for (size_t i = 0; i < countSize; ++i)
    {
        free(ptrMalloc[i]);
        pool.freeMemory(ptrPool[i]);
    }
    pool.collectStatistic();

    std::cout << "----------------Test_4 END-----------------" << std::endl;
    return true;
}



bool Test_5()
{
    std::cout << "----------------Test_5 (Medium allocation. Compare)-----------------" << std::endl;

    //create 32k-64k rendom allcation increase size, after that randomly delete it
    mem::MemoryPool pool(g_pageSize, &g_allocator);

    const size_t minMallocSize = 32'769;
    const size_t maxMallocSize = g_pageSize;  //64 KB

    const size_t testSize = 32'000;
    std::vector<std::pair<size_t, size_t>> sizes(testSize);
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(minMallocSize, maxMallocSize);

        for (size_t i = 0; i < sizes.size(); ++i)
        {
            size_t sz = dis(gen);
            sizes[i] = { i, sz };
        }
    }

    std::vector<std::pair<size_t, size_t>> sizesShuffle(testSize);
    {
        std::copy(sizes.begin(), sizes.end(), sizesShuffle.begin());
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(sizesShuffle.begin(), sizesShuffle.end(), gen);
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        std::vector<void*> pointers(testSize);
        for (size_t i = 0; i < sizes.size(); ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* volatile ptr = pool.allocMemory(sizes[i].second);
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr != nullptr);
            memset(ptr, (int)i, sizes[i].second);
            pointers[i] = ptr;
        }
        pool.collectStatistic();

        for (size_t i = 0; i < sizesShuffle.size(); ++i)
        {
            auto& val = sizesShuffle[i];
            volatile void* volatile ptr = pointers[val.first];
            assert(ptr != nullptr);
            auto startTime1 = std::chrono::high_resolution_clock::now();
            pool.freeMemory((void*)ptr);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }
        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        std::vector<void*> pointers(testSize);
        for (size_t i = 0; i < sizes.size(); ++i)
        {
            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* volatile ptr = malloc(sizes[i].second);
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr != nullptr);
            memset(ptr, (int)i, sizes[i].second);
            pointers[i] = ptr;
        }

        for (size_t i = 0; i < sizesShuffle.size(); ++i)
        {
            auto& val = sizesShuffle[i];
            volatile void* volatile ptr = pointers[val.first];
            assert(ptr != nullptr);
            auto startTime1 = std::chrono::high_resolution_clock::now();
            free((void*)ptr);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_5 END-----------------" << std::endl;
    return true;
}

bool Test_6()
{
    std::cout << "----------------Test_6 (Medium allocation. Compare Random Alloc/Dealloc memory)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize, &g_allocator);

    const size_t minMallocSize = 32'736;
    const size_t maxMallocSize = g_pageSize;

    const int countEvents = 100'000;
    std::vector<std::tuple<int, size_t, size_t>> events;

    //generate event
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> eventDst(0, 1000);

    std::vector<size_t> eventElements;
    for (size_t i = 0; i < countEvents; ++i)
    {
        int id = eventDst(gen) % 4;
        switch (id)
        {
        case 0: //create
        case 1:
        {
            static int counter = 0;
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dst(minMallocSize, maxMallocSize);

            size_t size = dst(gen);
            events.push_back({ 0, counter, size });
            eventElements.push_back(counter);
            ++counter;
        }
        break;

        case 2: //delete
        {
            if (eventElements.size() < 10)
            {
                break;
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dst(0, eventElements.size() - 1);

            size_t index = dst(gen);

            events.push_back({ 1, index, eventElements[index] });
            eventElements.erase(std::next(eventElements.begin(), index));
        }
        break;

        case 3: //memcpy
        case 4:
        {
            if (eventElements.empty())
            {
                break;
            }

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dst(0, eventElements.size() - 1);

            size_t index = dst(gen);

            events.push_back({ 2, index, eventElements[index] });
        }
        break;

        default:
        {
            int t = 0;
        }
        }
    }

    //pool
    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        std::vector<std::pair<void* volatile, size_t>> pointers;

        for (size_t i = 0; i < events.size(); ++i)
        {
            switch (std::get<0>(events[i]))
            {
            case 0: //create
            {
                size_t size = std::get<2>(events[i]);
                auto startTime = std::chrono::high_resolution_clock::now();
                void* volatile ptr = pool.allocMemory(size);
                auto endTime = std::chrono::high_resolution_clock::now();
                allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                assert(ptr);

                pointers.push_back({ ptr, size });
            }
            break;

            case 1: //delete
            {
                size_t index = std::get<2>(events[i]);
                void* volatile ptr = pointers[index].first;
                assert(ptr);
                auto startTime = std::chrono::high_resolution_clock::now();
                pool.freeMemory(ptr);
                auto endTime = std::chrono::high_resolution_clock::now();
                deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                pointers[index].first = nullptr;
            }
            break;

            case 2:
            {
                size_t index = std::get<2>(events[i]);
                void* volatile ptr = pointers[index].first;
                size_t size = pointers[index].second;
                assert(ptr);

                memset(ptr, (int)index, size);
            }
            break;
            }
        }

        for (size_t i = 0; i < pointers.size(); ++i)
        {
            if (pointers[i].first)
            {
                void* volatile ptr = pointers[i].first;
                assert(ptr);
                auto startTime = std::chrono::high_resolution_clock::now();
                pool.freeMemory(ptr);
                auto endTime = std::chrono::high_resolution_clock::now();
                deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
            }
        }

        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    //malloc
    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        std::vector<std::pair<void* volatile, size_t>> pointers;

        for (size_t i = 0; i < events.size(); ++i)
        {
            switch (std::get<0>(events[i]))
            {
            case 0: //create
            {
                size_t size = std::get<2>(events[i]);
                auto startTime = std::chrono::high_resolution_clock::now();
                void* volatile ptr = malloc(size);
                auto endTime = std::chrono::high_resolution_clock::now();
                allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                assert(ptr);

                pointers.push_back({ ptr, size });
            }
            break;

            case 1: //delete
            {
                size_t index = std::get<2>(events[i]);
                void* volatile ptr = pointers[index].first;
                assert(ptr);
                auto startTime = std::chrono::high_resolution_clock::now();
                free(ptr);
                auto endTime = std::chrono::high_resolution_clock::now();
                deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                pointers[index].first = nullptr;
            }
            break;

            case 2:
            {
                size_t index = std::get<2>(events[i]);
                void* volatile ptr = pointers[index].first;
                size_t size = pointers[index].second;
                assert(ptr);

                memset(ptr, (int)index, size);
            }
            break;
            }
        }

        for (size_t i = 0; i < pointers.size(); ++i)
        {
            if (pointers[i].first)
            {
                void* volatile ptr = pointers[i].first;
                assert(ptr);
                auto startTime = std::chrono::high_resolution_clock::now();
                free(ptr);
                auto endTime = std::chrono::high_resolution_clock::now();
                deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
            }
        }

        std::cout << "Malloc/free stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_6 END-----------------" << std::endl;
    return true;
}

bool Test_7()
{
    std::cout << "----------------Test_7 (Large allocation)-----------------" << std::endl;

    mem::MemoryPool pool(g_pageSize, &g_allocator);
    const size_t minMallocSize = g_pageSize;
    const size_t maxMallocSize = 1024 * 1024 * 256;  //256 MB

    const size_t countTest = 20;
    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        std::vector<std::pair<void* volatile, size_t>> mem(countTest);
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dis(minMallocSize, maxMallocSize);

            for (size_t i = 0; i < mem.size(); ++i)
            {
                size_t sz = dis(gen);
                auto startTime = std::chrono::high_resolution_clock::now();
                void* volatile ptr = pool.allocMemory(sz);
                auto endTime = std::chrono::high_resolution_clock::now();
                allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                mem[i] = { ptr, sz };
                memset(ptr, (int)i + 1, sz);
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
                auto startTime = std::chrono::high_resolution_clock::now();
                pool.freeMemory(mem[i].first);
                auto endTime = std::chrono::high_resolution_clock::now();
                deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
            }
            pool.collectStatistic();
        }

        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        std::vector<std::pair<void* volatile, size_t>> mem(countTest);
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dis(minMallocSize, maxMallocSize);

            for (size_t i = 0; i < mem.size(); ++i)
            {
                size_t sz = dis(gen);
                auto startTime = std::chrono::high_resolution_clock::now();
                void* volatile ptr = malloc(sz);
                auto endTime = std::chrono::high_resolution_clock::now();
                allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                mem[i] = { ptr, sz };
                memset(ptr, (int)i + 1, sz);
                assert(mem[i].first != nullptr);
            }
        }

        {
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
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
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
    mem::MemoryPool pool(g_pageSize, &g_allocator);

    const size_t maxMallocSize = 10 * 1024 * 1024;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1024, maxMallocSize);

    std::vector<std::pair<size_t, void* volatile>> sizesPool;
    std::vector<void*> sizesDefault;

    const size_t countAllocation = 1000;
    for (size_t i = 0; i < countAllocation; ++i)
    {
        size_t sz = dis(gen);
        sizesPool.push_back(std::make_pair(sz, nullptr));
    }

    for (auto& size : sizesPool)
    {
        size.second = pool.allocMemory(size.first);
        assert(size.second);
        memset(size.second, (int)size.first, size.first);

        auto startTime = std::chrono::high_resolution_clock::now();
        void* volatile ptr = malloc(size.first);
        auto endTime = std::chrono::high_resolution_clock::now();
        allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        assert(ptr);
        memset(ptr, (int)size.first, size.first);
        sizesDefault.push_back((void*)ptr);
    }
    std::cout << "After Create" << std::endl;
    pool.collectStatistic();

    for (size_t i = 0; i < sizesPool.size(); ++i)
    {
        int d = memcmp(sizesPool[i].second, (void*)sizesDefault[i], sizesPool[i].first);
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
        free((void*)size);
        auto endTime = std::chrono::high_resolution_clock::now();
        deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    }

    std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;

    std::cout << "----------------Test_8 END-----------------" << std::endl;
    return true;
}

bool Test_9()
{
    std::cout << "----------------Test_9 (Large Allocation. 2 alloc many times)-----------------" << std::endl;

    const size_t countIter = 1'000;

    const size_t maxMallocSize = 1024 * 1024 * 512;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(g_pageSize, maxMallocSize);
    size_t size1 = 0;
    size_t size2 = 0;

    {
        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < countIter; ++i)
        {
            size1 = dis(rd);
            size2 = dis(rd);

            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* volatile ptr1 = malloc(size1);
            void* volatile ptr2 = malloc(size2);
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);
            memset(ptr1, (int)i, size1);
            memset(ptr2, (int)i, size2);

            auto startTime1 = std::chrono::high_resolution_clock::now();
            free(ptr1);
            free(ptr2);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }

        std::cout << "STD malloc: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    {
        mem::MemoryPool pool(g_pageSize, &g_allocator);
        pool.preAllocatePools();

        mem::u64 allocateTime = 0;
        mem::u64 deallocateTime = 0;

        for (size_t i = 0; i < countIter; ++i)
        {
            size1 = dis(rd);
            size2 = dis(rd);

            auto startTime0 = std::chrono::high_resolution_clock::now();
            void* volatile ptr1 = pool.allocMemory(size1);
            void* volatile ptr2 = pool.allocMemory(size2);
            auto endTime0 = std::chrono::high_resolution_clock::now();
            allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime0 - startTime0).count();

            assert(ptr1 != nullptr);
            assert(ptr2 != nullptr);
            memset(ptr1, (int)i, size1);
            memset(ptr2, (int)i, size2);

            auto startTime1 = std::chrono::high_resolution_clock::now();
            pool.freeMemory(ptr2);
            pool.freeMemory(ptr1);
            auto endTime1 = std::chrono::high_resolution_clock::now();
            deallocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime1 - startTime1).count();
        }
        pool.collectStatistic();
        std::cout << "POOL stat: (ms)" << (double)allocateTime / 1000.0 << " / " << (double)deallocateTime / 1000.0 << std::endl;
    }

    std::cout << "----------------Test_2 END-----------------" << std::endl;
    return true;
}


int main()
{
#ifdef WIN32
    SYSTEM_INFO systemInfo;
    memset(&systemInfo, 0, sizeof(SYSTEM_INFO));
    ::GetSystemInfo(&systemInfo);

    g_pageSize = std::max<mem::u64>(mem::MemoryPool::k_mixSizePageSize, systemInfo.dwAllocationGranularity);
#endif //WIN32

#if __ANDROID__
    android_log::start_logger("MemoryPool Test");

    g_pageSize = std::max<mem::u64>(mem::MemoryPool::k_mixSizePageSize, (mem::u64)sysconf(_SC_PAGESIZE));
#endif //__ANDROID__
    std::cout << "PaseSize : " << g_pageSize << std::endl;

    TEST(Test_0());
    TEST(Test_1());
    TEST(Test_2());
    TEST(Test_3());
    TEST(Test_4());
    TEST(Test_5());
    TEST(Test_6());
    TEST(Test_7());
    //TEST(Test_8());
    TEST(Test_9());

    return 0;
}

#ifdef __ANDROID__
void handle_cmd(android_app* app, int32_t cmd) 
{
    switch (cmd) 
    {
    case APP_CMD_INIT_WINDOW:
        if (app->window != NULL) 
        {
            std::cout << "Start main" << std::endl;
            main();
            std::cout << "End main" << std::endl;
        }
        break;
    }
}

void android_main(android_app* app)
{
    __android_log_print(ANDROID_LOG_DEBUG, "MemoryPool Test", "android_main");
    app->userData = nullptr;
    app->onAppCmd = handle_cmd;

    while (true)
    {
        int ident;
        int events;
        android_poll_source* source;

        while ((ident = ALooper_pollAll(0, NULL, &events,(void**)&source)) >= 0) 
        {
            if (source != NULL) 
            {
                source->process(app, source);
            }
        }
    }
}
#endif //__ANDROID__