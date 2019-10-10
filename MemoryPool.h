#pragma once

#include <vector>
#include <list>
#include <map>

#define DEBUG_MEMORY 1
#define ENABLE_STATISTIC 1

#ifdef new
#   undef new
#endif

#ifdef delete
#   undef delete
#endif 

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
        explicit MemoryPool(u64 pageSize, MemoryAllocator* allocator = MemoryPool::getDefaultMemoryAllocator(), void* user = nullptr);

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

        struct Pool;
        struct PoolTable;

        struct Block
        {
            Block()
                : _pool(nullptr)
                , _next(nullptr)
                , _prev(nullptr)
                , _size(0)
#if DEBUG_MEMORY
                , _ptr(nullptr)
#endif //DEBUG_MEMORY
            {

            }

            void reset()
            {
                _next = nullptr;
                _prev = nullptr;
            }

            Pool*       _pool;
            Block*      _next;
            Block*      _prev;
            u64         _size;
#if DEBUG_MEMORY
            address_ptr _ptr;
#endif //DEBUG_MEMORY
            address_ptr ptr()
            {
                u64 offset = sizeof(Block);
                address_ptr ptr = (address_ptr)(reinterpret_cast<u64>(this) + offset);
                return ptr;
            }
        };

        class BlockList
        {
        public:

            BlockList()
                : _end(nullptr)
            {
                clear();
            }

            void clear()
            {
                if (_end)
                {
                    delete _end;
                    _end = nullptr;
                }

                _end = new Block();
                link(_end, _end);
            }

            void insert(Block* block)
            {
                link(_end->_prev, block);
                link(block, _end);
            }

            void erase(Block* block)
            {
                link(block->_prev, block->_next);
            }

            bool empty()
            {
                return _end == _end->_next;
            }

        private:

            void link(Block* l, Block* r)
            {
                l->_next = r;
                r->_prev = l;
            }

            Block* _end;
        };

        struct Pool
        {
            PoolTable*          _table;
            BlockList           _used;
            std::vector<Block*> _free;
        };

        struct PoolTable
        {
            void init(u32 size);

            std::list<Pool*> _pools;
            u32              _size;
        };

        std::vector<PoolTable> m_smallPoolTables;
#if DEBUG_MEMORY
        BlockList m_largeAllocations;
#endif //DEBUG_MEMORY
        void*   m_userData;
        u64     m_pageSize;

        static MemoryAllocator* s_defaultMemoryAllocator;

        Pool* allocatePool(PoolTable* table, u32 align);
        void deallocatePool(Pool* pool);
#if ENABLE_STATISTIC
        void collectStatistic();
#endif //ENABLE_STATISTIC
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
