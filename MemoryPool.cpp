#include "MemoryPool.h"

#include <memory>
#include <map>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <stdlib.h>
#include <type_traits>

#ifdef new
#   undef new
#endif

#ifdef delete
#   undef delete
#endif 

#ifdef malloc
#   undef malloc
#endif

#ifdef free
#   undef free
#endif

namespace mem
{
    enum
    {
        DEFAULT_ALIGMENT = 4,

        MIN_ALIGMENT = 4,
        MAX_ALIGMENT = 16,
    };

    template<class T>
    inline T alignUp(T val, T alignment)
    {
        return (val + alignment - 1) & ~(alignment - 1);
    }

    static std::vector<u16> s_smalBlockTableSizes =
    {
        16, 32, 48, 64, 80, 96, 112, 128,
        160, 192, 224, 256, 288, 320, 384, 448,
        512, 576, 640, 704, 768, 896, 1024, 1168,
        1360, 1632, 2048, 2336, 2720, 3264, 4096, 4368,
        4672, 5040, 5456, 5952, 6544, 7280, 8192, 9360,
        10912, 13104, 16384, 21840, 32768
    };

    MemoryPool::MemoryAllocator* MemoryPool::s_defaultMemoryAllocator = nullptr;

    MemoryPool::MemoryPool(u64 pageSize, MemoryAllocator* allocator, void* user) noexcept
        : m_allocator(allocator)
        , m_userData(user)
        , k_pageSize(pageSize)
        , k_maxSizePoolAllocation(pageSize)

        , k_deleteUnusedPools(true)
    {

        assert(k_pageSize >= 65'536);
        m_smallTableIndex.fill(0);
        m_smallPoolTables.resize(s_smalBlockTableSizes.size(), {});

        u32 blockIndex = 0;
        auto blockIter = s_smalBlockTableSizes.cbegin();
        for (u64 i = 0; i < (k_maxSizeSmallTableAllocation >> 2); ++i)
        {
            u64 blockSize = (u64)((i + 1U) << 2U);
            while (blockIter != s_smalBlockTableSizes.cend() && *blockIter < blockSize)
            {
                ++blockIndex;
                blockIter = std::next(blockIter);
            }

            m_smallTableIndex[i] = blockIndex;
            m_smallPoolTables[blockIndex]._size = *blockIter;
            m_smallPoolTables[blockIndex]._type = PoolTable::SmallTable;
        }

        m_poolTable._size = alignUp<u64>(k_maxSizePoolAllocation * k_countPagesPerAllocation, DEFAULT_ALIGMENT);

        //pre init
        //MemoryPool::preAllocatePools()

        m_markedToDelete.reserve(32);
    }

    MemoryPool::~MemoryPool()
    {
        MemoryPool::clear();
        m_userData = nullptr;
    }

    address_ptr MemoryPool::allocMemory(u64 size, u32 aligment)
    {
#if ENABLE_STATISTIC
        auto startTime = std::chrono::high_resolution_clock::now();
#endif //ENABLE_STATISTIC
        assert(size);
        if (aligment == 0) //default
        {
            aligment = DEFAULT_ALIGMENT;
        }

        u32 aligmentedSize = alignUp<u32>(static_cast<u32>(size), aligment);
        if (aligmentedSize <= k_maxSizeSmallTableAllocation && aligment == DEFAULT_ALIGMENT)
        {
            //small allocations
            Block* block = allocateFromSmallTables(aligmentedSize);
            assert(block);
            address_ptr ptr = block->ptr();
#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<0>(block->_size);
#endif //ENABLE_STATISTIC

            return ptr;
        }
        else if (aligmentedSize <= k_maxSizePoolAllocation && aligment == DEFAULT_ALIGMENT)
        {
            //pool allocation
            Block* block = allocateFromTable(aligmentedSize);
            assert(block);
            address_ptr ptr = block->ptr();
#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<1>(block->_size);
#endif //ENABLE_STATISTIC

            return ptr;
        }
        else
        {
            //large allocation
            u64 allocationSize = alignUp<u64>(aligmentedSize + sizeof(Block), aligment);
            address_ptr memory = m_allocator->allocate(allocationSize, aligment, m_userData);
            assert(memory);

            Block* block = initBlock(memory, nullptr, allocationSize);
#if DEBUG_MEMORY
            m_largeAllocations.insert(block);
#endif
#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<2>(allocationSize);
            m_statistic.registerPoolAllocation<2>(allocationSize);
#endif //ENABLE_STATISTIC

            address_ptr ptr = block->ptr();
            return ptr;
        }

        assert(false);
        return nullptr;
    }

    void MemoryPool::freeMemory(address_ptr memory)
    {
#if ENABLE_STATISTIC
        auto startTime = std::chrono::high_resolution_clock::now();
#endif //ENABLE_STATISTIC
        address_ptr ptr = (address_ptr)(reinterpret_cast<u64>(memory) - sizeof(Block));
        
        Block* block = (Block*)ptr;
        assert(block);
        if (block->_pool)
        {
            freeBlock(block);
        }
        else
        {
#if DEBUG_MEMORY
            assert(!m_largeAllocations.empty() && "empty");
            m_largeAllocations.erase(block);
#endif
            u64 blockSize = block->_size;
            m_allocator->deallocate(ptr, blockSize, m_userData);
#if ENABLE_STATISTIC
            m_statistic.registerDeallocation<2>(blockSize);
            m_statistic.registerPoolDeallocation<2>(blockSize);
#endif //ENABLE_STATISTIC
        }

#if ENABLE_STATISTIC
        auto endTime = std::chrono::high_resolution_clock::now();
        m_statistic._dealocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
#endif //ENABLE_STATISTIC
    }

    void MemoryPool::preAllocatePools()
    {
        for (auto& table : m_smallPoolTables)
        {
            assert(table._pools.empty());
            Pool* pool = MemoryPool::allocateFixedBlocksPool(&table, DEFAULT_ALIGMENT);
            table._pools.push_back(pool);
        }
    }

    void MemoryPool::clear()
    {
        for (auto& table : m_smallPoolTables)
        {
            auto poolIter = table._pools.begin();
            while (poolIter != table._pools.end())
            {
                assert((*poolIter)->_used.empty()); // used elements
                (*poolIter)->reset();

                MemoryPool::deallocatePool(*poolIter);

                ++poolIter;
            }
            table._pools.clear();
        }


        assert(m_largeAllocations.empty()); //used elements
        for (auto& block : m_largeAllocations)
        {
            u64 blockSize = block._size;
            m_allocator->deallocate(&block, blockSize, m_userData);
        }
        m_largeAllocations.clear();

#if ENABLE_STATISTIC
        m_statistic.reset();
#endif //ENABLE_STATISTIC
    }

    MemoryPool::MemoryAllocator* MemoryPool::getDefaultMemoryAllocator()
    {
        if (!s_defaultMemoryAllocator)
        {
            s_defaultMemoryAllocator = new DefaultMemoryAllocator();
        }
        return s_defaultMemoryAllocator;
    }

    MemoryPool::Pool* MemoryPool::allocateFixedBlocksPool(PoolTable* table, u32 align)
    {
        u64 blockSize = table->_size + sizeof(Block);
        u64 countAllocations = ((k_maxSizePoolAllocation * k_countPagesPerAllocation) - sizeof(Pool)) / blockSize;
        u64 allocatedSize = alignUp<u64>(k_maxSizePoolAllocation, (countAllocations * blockSize) + sizeof(Pool));

        address_ptr memory = m_allocator->allocate(allocatedSize, align, m_userData);
        assert(memory);

        Pool* pool = new(memory) Pool(table, table->_size, allocatedSize);
        u64 memoryOffset = reinterpret_cast<u64>(pool->block());

#if ENABLE_STATISTIC
        m_statistic.registerPoolAllocation<0>(allocatedSize);
#endif //ENABLE_STATISTIC

        for (u32 i = 0; i < countAllocations; ++i)
        {
            address_ptr memoryBlock = reinterpret_cast<address_ptr>(memoryOffset + (i * (table->_size + sizeof(Block))));
            Block* block = initBlock(memoryBlock, pool, blockSize);
            pool->_free.insert(block);
        }

        return pool;
    }

    MemoryPool::Pool* MemoryPool::allocatePool(PoolTable* table, u32 align)
    {
        u64 allocatedSize = alignUp<u64>(k_maxSizePoolAllocation * k_countPagesPerAllocation, align);
        assert(table->_size == allocatedSize); //different aligment
        address_ptr memory = m_allocator->allocate(allocatedSize, align, m_userData);
        assert(memory);

        Pool* pool = new(memory) Pool(table, 0, allocatedSize);

#if ENABLE_STATISTIC
        m_statistic.registerPoolAllocation<1>(allocatedSize);
#endif //ENABLE_STATISTIC

        Block* block = initBlock(pool->block(), pool, allocatedSize - sizeof(Pool));
        pool->_free.insert(block);

        return pool;
    }

    void MemoryPool::deallocatePool(Pool* pool)
    {
        assert(pool);
        assert(pool->_used.empty());
        m_allocator->deallocate(pool, pool->_poolSize, m_userData);
    }

    MemoryPool::Block* MemoryPool::initBlock(address_ptr ptr, Pool* pool, u64 size)
    {
        assert(size >= sizeof(Block));
        Block* block = new(ptr)Block();
#if DEBUG_MEMORY
        block->reset();
        block->_ptr = (address_ptr)(reinterpret_cast<u64>(ptr) + sizeof(Block));
#endif //DEBUG_MEMORY
        block->_size = size;
        block->_pool = pool;

        return block;
    }

    void MemoryPool::freeBlock(Block* block)
    {
        Pool* pool = block->_pool;
        pool->_used.erase(block);

        if (pool->_table->_type == PoolTable::SmallTable)
        {
            assert(block->_size == pool->_table->_size + sizeof(Block));
            pool->_free.insert(block);
#if ENABLE_STATISTIC
            m_statistic.registerDeallocation<0>(block->_size);
#endif //ENABLE_STATISTIC

            if (k_deleteUnusedPools)
            {
                bool skip = true;
                auto& pools = const_cast<decltype(pool->_table->_pools)&>(pool->_table->_pools);

                m_markedToDelete.clear();
                for (auto iter = pools.begin(); iter != pools.end();)
                {
                    if ((*iter)->_used.empty())
                    {
                        if (skip) //skip first
                        {
                            skip = false;
                        }
                        else
                        {
                            m_markedToDelete.push_back(*iter);
                            iter = pools.erase(iter);

                            continue;
                        }
                    }
                    ++iter;
                }

                if (m_markedToDelete.size() > 0)
                {
                    for (auto& pool : m_markedToDelete)
                    {
#if ENABLE_STATISTIC
                        m_statistic.registerPoolDeallocation<0>(pool->_poolSize);
#endif //ENABLE_STATISTIC
                        MemoryPool::deallocatePool(pool);
                    }
                    m_markedToDelete.clear();
                }
            }
        }
        else
        {
            assert(pool->_table->_type == PoolTable::Default);
            u64 blockSize = block->_size;
            //pool->_free.insert(block);
            pool->_free.priorityInsert(block);
            pool->_free.merge();

            assert(pool->_blockSize >= blockSize);
            pool->_blockSize -= blockSize;
#if ENABLE_STATISTIC
            m_statistic.registerDeallocation<1>(blockSize);
#endif //ENABLE_STATISTIC

            if (k_deleteUnusedPools)
            {
                deleteEmptyPools();
            }
        }
    }

    MemoryPool::Block* MemoryPool::allocateFromSmallTables(u64 aligmentedSize)
    {
        assert(aligmentedSize <= std::numeric_limits<u32>::max());
        u32 index = (static_cast<u32>(aligmentedSize) >> 2) - 1;
        u32 tableIndex = m_smallTableIndex[index];

        PoolTable& table = m_smallPoolTables[tableIndex];
        if (table._pools.empty())
        {
            Pool* pool = MemoryPool::allocateFixedBlocksPool(&table, DEFAULT_ALIGMENT);
            table._pools.push_back(pool);
        }

        Block* block = nullptr;
        for (auto& pool : table._pools)
        {
            if (!pool->_free.empty())
            {
                Block* block = pool->_free.begin();
                if (block != pool->_free.end())
                {
                    assert(block->_size == pool->_blockSize + sizeof(Block));
                    pool->_free.erase(block);
                    pool->_used.insert(block);

                    return block;
                }
            }
        }

        //allocate new pool
        Pool* pool = MemoryPool::allocateFixedBlocksPool(&table, DEFAULT_ALIGMENT);
        table._pools.push_back(pool);

        block = pool->_free.begin();
        pool->_free.erase(block);
        pool->_used.insert(block);

        return block;
    }

    MemoryPool::Block* MemoryPool::allocateFromTable(u64 aligmentedSize)
    {
        PoolTable& table = m_poolTable;
        for (auto& pool : table._pools)
        {
            if (!pool->_free.empty())
            {
                Block* block = pool->_free.begin();
                while (block != pool->_free.end())
                {
                    u64 requestedSize = aligmentedSize + sizeof(Block);
                    if (block->_size >= requestedSize)
                    {
                        u64 freeMemory = block->_size - requestedSize;
                        if (freeMemory > k_maxSizeSmallTableAllocation + sizeof(Block))
                        {
                            block->_size = requestedSize;

                            address_ptr emptyMemory = (address_ptr)(reinterpret_cast<u64>(block) + requestedSize);
                            Block* emptyBlock = initBlock(emptyMemory, pool, freeMemory);
                            pool->_free.priorityInsert(emptyBlock);
                        }
                        pool->_free.erase(block);
                        pool->_used.priorityInsert(block);
                        pool->_blockSize += block->_size;
                        assert(pool->_blockSize <= pool->_poolSize);

                        return block;
                    }
                    block = block->_next;
                }
            }
        }

        //create new pool
        Pool* pool = MemoryPool::allocatePool(&table, DEFAULT_ALIGMENT);
        table._pools.push_back(pool);

        Block* block = pool->_free.begin();
        assert(block != pool->_free.end());

        u64 requestedSize = aligmentedSize + sizeof(Block);
        assert(block->_size >= requestedSize);

        u64 freeMemory = block->_size - requestedSize;
        if (freeMemory > k_maxSizeSmallTableAllocation + sizeof(Block))
        {
            block->_size = requestedSize;

            address_ptr emptyMemory = (address_ptr)(reinterpret_cast<u64>(pool->block()) + requestedSize);
            Block* emptyBlock = initBlock(emptyMemory, pool, freeMemory);
            pool->_free.priorityInsert(emptyBlock);
        }

        pool->_free.erase(block);
        pool->_used.insert(block);
        pool->_blockSize += block->_size;
        assert(pool->_blockSize <= pool->_poolSize);

        return block;
    }

    void MemoryPool::deleteEmptyPools()
    {
        bool skip = true;
        m_markedToDelete.clear();

        auto& pools = m_poolTable._pools;
        for (size_t i = 0; i < pools.size();)
        {
            if (pools[i]->_used.empty())
            {
                if (skip) //skip first
                {
                    skip = false;
                }
                else
                {
                    m_markedToDelete.push_back(pools[i]);

                    auto lastIter = pools.rbegin();
                    std::swap(pools[i], *lastIter);
                    pools.pop_back();

                    continue;
                }
            }
            ++i;
        }

        if (m_markedToDelete.size() > 0)
        {
            for (auto& pool : m_markedToDelete)
            {
#if ENABLE_STATISTIC
                m_statistic.registerPoolDeallocation<1>(pool->_poolSize);
#endif //ENABLE_STATISTIC
                MemoryPool::deallocatePool(pool);
            }
            m_markedToDelete.clear();
        }
    }

    void MemoryPool::collectStatistic()
    {
#if ENABLE_STATISTIC
        std::cout << "Pool Statistic" << std::endl;
        std::cout << "Time alloc/dealloc (ms): " << (f64)m_statistic._allocateTime / 1000.0 << "/" << (f64)m_statistic._dealocateTime / 1000.0 << std::endl;
        std::cout << "Count Allocation : " << m_statistic._generalAllocationCount << ". Size (byte): " << m_statistic._generalAllocationSize << std::endl;
        std::cout << "Pool Staticstic:" << std::endl;
        std::cout << " SmallTable - Sizes/PoolSizes (byte): " << m_statistic._tableAllocationSizes[0] << "/" << m_statistic._poolAllocationSizes[0] 
            << " Count Allocations/Pools: " << m_statistic._tableAllocationCount[0] << "/" << m_statistic._poolsAllocationCount[0] << std::endl;
        std::cout << " PoolTable - Sizes/PoolSizes (byte): " << m_statistic._tableAllocationSizes[1] << "/" << m_statistic._poolAllocationSizes[1]
            << " Count Allocations/Pools: " << m_statistic._tableAllocationCount[1] << "/" << m_statistic._poolsAllocationCount[1] << std::endl;
        std::cout << " LargeAllocations - Sizes/PoolSizes (byte): " << m_statistic._tableAllocationSizes[2] << "/" << m_statistic._poolAllocationSizes[2]
            << " Count Allocations/Pools: " << m_statistic._tableAllocationCount[2] << "/" << m_statistic._poolsAllocationCount[2] << std::endl;
#endif //ENABLE_STATISTIC
    }


    address_ptr DefaultMemoryAllocator::allocate(u64 size, u32 aligment, void* user)
    {
        address_ptr ptr = malloc(size);
        assert(ptr && "Invalid allocate");

#if DEBUG_MEMORY
        memset(ptr, 'X', size);
#endif //DEBUG_MEMORY
        return ptr;
    }

    void DefaultMemoryAllocator::deallocate(address_ptr memory, u64 size, void* user)
    {
        assert(memory && "Invalid block");
        free(memory);
    }

} //namespace mem
