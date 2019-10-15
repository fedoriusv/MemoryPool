#pragma once

#include <assert.h>
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
    typedef signed long long    s64;
    typedef unsigned int        u32;
    typedef void*               address_ptr;

    template<class T>
    inline T alignUp(T val, T alignment)
    {
        return (val + alignment - 1) & ~(alignment - 1);
    }

    const std::map<u32, u32> k_sizesTable =
    {
        { 4,  0 },
        { 8,  1 },
        { 12, 2 },
        { 16, 3 },
        { 24, 4 }
    };

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

    /*
    * class MemoryPool
    */
    template<bool SmallAlloc = true, bool LargeAlloc = true>
    class MemoryPool
    {
    public:

        MemoryPool(const MemoryPool&) = delete;
        MemoryPool& operator=(const MemoryPool&) = delete;

        /*
        * MemoryPool constuctor
        * param pageSize : page size
        * param allocator: allocator
        * param user: user data
        */
        explicit MemoryPool(u64 pageSize, MemoryAllocator* allocator = MemoryPool::getDefaultMemoryAllocator(), void* user = nullptr)
            : m_allocator(allocator)
            , m_userData(user)
            , m_pageSize(pageSize)
        {
            m_smallPoolTables.resize(k_sizesTable.size(), { {}, 0 });
            for (auto& [size, index] : k_sizesTable)
            {
                assert(index < m_smallPoolTables.size());
                m_smallPoolTables[index].init(size);
            }
        }

        /*
        * ~MemoryPool destuctor
        */
        ~MemoryPool()
        {
            MemoryPool::clear();
            m_userData = nullptr;
        }

        /*
        * Request free memory from pool
        * param size: count bites will be requested
        */
        address_ptr allocMemory(u64 size, u32 aligment = 0)
        {
            if (aligment == 0) //default
            {
                aligment = 4;
            }

            u64 aligmentSize = alignUp<u64>(static_cast<u64>(size), aligment);
            if constexpr (SmallAlloc)
            {
                //small allocation
                address_ptr ptr = smallTableAllocation(aligmentSize, aligment);
                if (ptr)
                {
                    return ptr;
                }

                return middleTableAllocation(aligmentSize, aligment);
            }
            else if constexpr (LargeAlloc)
            {
                //large allocation
                return largeAllocation(aligmentSize, aligment);
            }

            return middleTableAllocation(aligmentSize, aligment);
        }

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
        void freeMemory(address_ptr memory)
        {
#if ENABLE_STATISTIC
            auto startTime = std::chrono::high_resolution_clock::now();
#endif //ENABLE_STATISTIC
            address_ptr ptr = (address_ptr)(reinterpret_cast<u64>(memory) - sizeof(Block));
            Block* block = (Block*)ptr;
            assert(block);
            if (block->_pool)
            {
                u64 blockSize = block->_size;

                Pool* pool = block->_pool;
                pool->_used.erase(block);

                block->reset();
                pool->_free.push_back(block);
#if ENABLE_STATISTIC
                auto endTime = std::chrono::high_resolution_clock::now();
                m_statistic._dealocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                m_statistic.registerDeallocation<0>(blockSize);
#endif //ENABLE_STATISTIC

                std::list<Pool*> markTodelete;
                for (auto& cpool : pool->_table->_pools)
                {

                    if (cpool->_used.empty())
                    {
                        markTodelete.push_back(cpool);
                    }
                }

                if (markTodelete.size() > 1)
                {
                    for (auto& iter = std::next(markTodelete.begin(), 1); iter != markTodelete.end(); ++iter)
                    {
                        MemoryPool::deallocatePool(*iter);
                    }
                }
            }
            else
            {
#if DEBUG_MEMORY
                assert(!m_largeAllocations.empty() && "empty");
                m_largeAllocations.erase(block);
#endif //DEBUG_MEMORY
                u64 blockSize = block->_size;
                m_allocator->deallocate(ptr, blockSize, m_userData);
#if ENABLE_STATISTIC
                auto endTime = std::chrono::high_resolution_clock::now();
                m_statistic._dealocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                m_statistic.registerDeallocation<1>(blockSize);
#endif //ENABLE_STATISTIC
            }
        }

        /*
        * Free pool, Return all requested allocation
        */
        void clear()
        {
            //TODO
#if ENABLE_STATISTIC
            m_statistic.reset();
#endif //ENABLE_STATISTIC
        }

        static MemoryAllocator* getDefaultMemoryAllocator()
        {
            if (!s_defaultMemoryAllocator)
            {
                s_defaultMemoryAllocator = new DefaultMemoryAllocator();
            }
            return s_defaultMemoryAllocator;
        }

#if ENABLE_STATISTIC
        void collectStatistic()
        {
            std::cout << "Pool Statistic" << std::endl;
            std::cout << "Time alloc/dealloc (ms): " << (double)m_statistic._allocateTime / 1000.0 << " / " << (double)m_statistic._dealocateTime / 1000.0 << std::endl;
            std::cout << "Count Allocation : " << m_statistic._generalAllocationCount << ". Size (b): " << m_statistic._generalAllocationSize << std::endl;
            std::cout << "Pool Statistic" << std::endl;
        }
#endif //ENABLE_STATISTIC

    private:

        address_ptr smallTableAllocation(u64 aligmentSize, u32 aligment)
        {
#if ENABLE_STATISTIC
            auto startTime = std::chrono::high_resolution_clock::now();
#endif //ENABLE_STATISTIC
            if (aligmentSize <= m_smallPoolTables.back()._size)
            {
                u32 index = k_sizesTable.at(static_cast<u32>(aligmentSize));
                PoolTable& table = m_smallPoolTables[0];
                if (table._pools.empty())
                {
                    Pool* pool = MemoryPool::allocatePool(&table, aligment);
                    table._pools.push_back(pool);
                }

                Block* block = nullptr;
                for (auto& pool : table._pools)
                {
                    if (pool->_free.empty())
                    {
                        continue;
                    }

                    block = pool->_free.back();
                    pool->_free.pop_back();

                    pool->_used.insert(block);
#if ENABLE_STATISTIC
                    auto endTime = std::chrono::high_resolution_clock::now();
                    m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                    m_statistic.registerAllocation<0>(aligmentSize);
#endif //ENABLE_STATISTIC
                    return block->ptr();
                }

                Pool* pool = MemoryPool::allocatePool(&table, aligment);
                table._pools.push_back(pool);

                block = pool->_free.back();
                pool->_free.pop_back();

                pool->_used.insert(block);

#if ENABLE_STATISTIC
                auto endTime = std::chrono::high_resolution_clock::now();
                m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                m_statistic.registerAllocation<0>(aligmentSize);
#endif //ENABLE_STATISTIC
                return block->ptr();
            }

            return nullptr;
        }

        address_ptr middleTableAllocation(u64 aligmentSize, u32 aligment)
        {
#if ENABLE_STATISTIC
            auto startTime = std::chrono::high_resolution_clock::now();
#endif //ENABLE_STATISTIC

            //TODO
            assert(false);

#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<0>(aligmentSize);
#endif //ENABLE_STATISTIC
            return nullptr;
        }

        address_ptr largeAllocation(u64 aligmentSize, u32 aligment)
        {
#if ENABLE_STATISTIC
            auto startTime = std::chrono::high_resolution_clock::now();
#endif //ENABLE_STATISTIC
            address_ptr ptr = m_allocator->allocate(aligmentSize + sizeof(Block), aligment, m_userData);

            Block* block = new(ptr)Block();
            //block->reset();
            block->_pool = nullptr;
            block->_size = aligmentSize;
#if DEBUG_MEMORY
            block->_ptr = (address_ptr)(reinterpret_cast<u64>(ptr) + sizeof(Block));
#endif //DEBUG_MEMORY

            m_largeAllocations.insert(block);
#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<1>(aligmentSize);
#endif //ENABLE_STATISTIC
            return block->ptr();
        }

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
                assert(ptr);
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
            void init(u32 size)
            {
                assert(_pools.empty());
                _size = size;
            }

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

        Pool* allocatePool(PoolTable* table, u32 align)
        {
            Pool* pool = new Pool();

            u64 countAllocations = m_pageSize / sizeof(Block);
            address_ptr mem = m_allocator->allocate(countAllocations * (table->_size + sizeof(Block)), align, m_userData);

            pool->_table = table;
            pool->_free.resize(countAllocations, nullptr);

            u64 offset = 0;
            for (auto iter = pool->_free.rbegin(); iter != pool->_free.rend(); ++iter)
            {
                Block*& block = (*iter);

                address_ptr ptr = (address_ptr)(reinterpret_cast<u64>(mem) + offset);
                block = new(ptr)Block();
                block->_pool = pool;
                block->_size = table->_size;
#if DEBUG_MEMORY
                block->_ptr = (address_ptr)(reinterpret_cast<u64>(ptr) + sizeof(Block));
#endif //DEBUG_MEMORY
                offset += table->_size + sizeof(Block);
            }

            return pool;
        }

        void deallocatePool(Pool* pool)
        {
            //TODO
        }

#if ENABLE_STATISTIC
        struct Statistic
        {
            Statistic()
            {
                memset(this, 0, sizeof(Statistic));
            }

            void reset()
            {
                _allocateTime = 0;
                _dealocateTime = 0;

                _tableAllocationCount = 0;
                _largeAllocationCount = 0;

                _generalAllocationCount = 0;
                _generalAllocationSize = 0;
            }

            template<u32 type>
            void registerAllocation(u64 size)
            {
                static_assert(type < 2);
                if constexpr (type == 0)
                {
                    ++_tableAllocationCount;
                    assert(_tableAllocationCount < std::numeric_limits<s64>().max());
                }
                else if constexpr (type == 1)
                {
                    ++_largeAllocationCount;
                    assert(_largeAllocationCount < std::numeric_limits<s64>().max());
                }
                ++_generalAllocationCount;
                assert(_generalAllocationCount < std::numeric_limits<s64>().max());
                _generalAllocationSize += size;
            }

            template<u32 type>
            void registerDeallocation(u64 size)
            {
                static_assert(type < 2);
                if constexpr (type == 0)
                {
                    --_tableAllocationCount;
                    assert(_tableAllocationCount >= 0);
                }
                else if constexpr (type == 1)
                {
                    --_largeAllocationCount;
                    assert(_largeAllocationCount >= 0);
                }
                --_generalAllocationCount;
                assert(_generalAllocationCount >= 0);
                _generalAllocationSize -= size;
            }

            u64 _allocateTime;
            u64 _dealocateTime;

            s64 _tableAllocationCount;
            s64 _largeAllocationCount;

            s64 _generalAllocationCount;
            u64 _generalAllocationSize;
        };

        Statistic m_statistic;
#endif //ENABLE_STATISTIC
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    class DefaultMemoryAllocator : public MemoryAllocator
    {
    public:

        DefaultMemoryAllocator() {};
        ~DefaultMemoryAllocator() {};

        address_ptr allocate(u64 size, u32 aligment = 0, void* user = nullptr) override;
        void        deallocate(address_ptr memory, u64 size = 0, void* user = nullptr) override;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    inline address_ptr DefaultMemoryAllocator::allocate(u64 size, u32 aligment, void* user)
    {
        address_ptr ptr = malloc(size);
        assert(ptr && "Invalid allocate");

#ifdef DEBUG_MEMORY
        memset(ptr, 'X', size);
#endif //DEBUG_MEMORY
        return ptr;
    }

    inline void DefaultMemoryAllocator::deallocate(address_ptr memory, u64 size, void* user)
    {
        assert(memory && "Invalid block");
        free(memory);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    template<bool SmallAlloc, bool LargeAlloc>
    MemoryAllocator* MemoryPool<SmallAlloc, LargeAlloc>::s_defaultMemoryAllocator = nullptr;

    /////////////////////////////////////////////////////////////////////////////////////////////////////

} //namespace mem
