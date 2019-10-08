#pragma once

namespace mem
{
    typedef unsigned long long  u64;
    typedef unsigned int        u32;
    typedef void*               address_ptr;

    /*
    * class MemoryPool
    */
    class MemoryPool
    {
    public:

        /*
        * class MemoryAllocator. Allocator for pool
        */
        class MemoryAllocator
        {
        public:

            MemoryAllocator() {};
            virtual ~MemoryAllocator() {};

            virtual address_ptr allocate(u64 size, u32 aligment = 0, void* user = nullptr) = 0;
            virtual void        deallocate(address_ptr memory, u64 size = 0, void* user = nullptr) = 0;
        };

        MemoryPool(const MemoryPool&) = delete;
        MemoryPool& operator=(const MemoryPool&) = delete;

        /*
        * class MemoryPool constuctor
        * param initMemorySize: size will be rezerved
        * param memoryChunkSize: chunck size for every allocation
        * param minMemorySizeToAllocate: when pool is full new page will be allocated
        */
        explicit MemoryPool(MemoryAllocator* allocator = MemoryPool::getDefaultMemoryAllocator(), void* user = nullptr);

        ~MemoryPool();

        /*
        * Request free memory from pool
        * param size: count bites will be requested
        */
        address_ptr allocMemory(u64 size, u32 aligment = 0);

        template<class T>
        T* allocElement()
        {
            return reinterpret_cast<T*>(allocMemory(sizeof(T)));
        }

        template<class T>
        T* allocArray(u64 count)
        {
            return reinterpret_cast<T*>(allocMemory(sizeof(T) * count));
        }

        /*
        * Return memory to pool
        * param address_ptr: address of memory
        */
        void        freeMemory(address_ptr memory);

        /*
        * Free pool, Return all requested allocation
        */
        void        clear();

        static MemoryAllocator* getDefaultMemoryAllocator();


    private:

        MemoryAllocator* m_allocator;

        struct Table
        {
            //TODO
            u32 _size;
        };

        void* m_userData;

        static MemoryAllocator* s_defaultMemoryAllocator;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    class DefaultMemoryAllocator : public MemoryPool::MemoryAllocator
    {
    public:

        DefaultMemoryAllocator() {};
        ~DefaultMemoryAllocator() {};

        address_ptr allocate(u64 size, u32 aligment = 0, void* user = nullptr) override;
        void        deallocate(address_ptr memory, u64 size = 0, void* user = nullptr) override;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////

} //namespace mem
