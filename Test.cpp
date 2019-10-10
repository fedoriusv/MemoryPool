#include "MemoryPool.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <assert.h>
#include <memory>

size_t g_pageSize;

bool Test_1()
{
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

        pool.freeMemory(a);
        pool.freeMemory(b);
        assert(true);
    }


    {
        int* a = pool.allocElement<int>();
        assert(a != nullptr);
        *a = 10;
        assert(*a == 10);

        pool.freeMemory(a);
        assert(a != nullptr);
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

        pool.freeMemory(a);
    }

    {
        int count = 10;
        int* a = pool.allocArray<int>(count);
        for (int i = 0; i < count; ++i)
        {
            assert(&a[i] != nullptr);
        }

        pool.freeMemory(a);
    }

    return true;
}

bool Test_2()
{
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
    assert(Test_1());
    assert(Test_2());

    return 0;
}