#include "MemoryPool.h"

#include <assert.h>
#include <memory>

bool Test_1()
{
    mem::MemoryPool pool;

    {
        void* a = pool.allocMemory(sizeof(int));
        assert(a != nullptr);
        int* b = reinterpret_cast<int*>(a);
        *b = 10;
        assert(*(int*)a == 10);

        pool.freeMemory(a);
        assert(a != nullptr);
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
    assert(Test_1());
    assert(Test_2());

    return 0;
}