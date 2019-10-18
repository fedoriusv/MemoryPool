#include "MemoryPool.h"

#include <memory>
#include <map>
#include <iostream>
#include <chrono>

namespace mem
{

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

    MemoryPool::MemoryAllocator* MemoryPool::s_defaultMemoryAllocator = nullptr;

    MemoryPool::MemoryPool(u64 pageSize, MemoryAllocator* allocator, void* user)
        : m_allocator(allocator)
        , m_userData(user)
        , m_maxSmallTableAllocation(0)
        , k_maxSizeTableAllocation(pageSize)
    {
        m_smallPoolTables.resize(k_sizesTable.size(), { 0, PoolTable::SmallTable });
        for (auto& [size, index] : k_sizesTable)
        {
            assert(index < m_smallPoolTables.size());
            m_smallPoolTables[index].init(size);
        }
        m_maxSmallTableAllocation = m_smallPoolTables.back()._size;
        assert(k_maxSizeTableAllocation >= 65536);
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
        if (aligment == 0) //default
        {
            aligment = 4;
        }

        u32 aligmentSize = alignUp<u32>(static_cast<u32>(size), aligment);
        if (false && aligmentSize <= m_maxSmallTableAllocation)
        {
            //TODO
            //small allocations
            u32 index = k_sizesTable.at(aligmentSize);
            PoolTable& table = m_smallPoolTables[0];
            if (table._pools.empty())
            {
                Pool* pool = nullptr;// MemoryPool::allocatePool(&table, aligment);
                table._pools.push_back(pool);
            }

            Block* block = nullptr;
            for (auto& pool : table._pools)
            {
                if (pool->_free.empty())
                {
                    continue;
                }

                /*block = pool->_free.back();
                pool->_free.pop_back();*/

                pool->_used.insert(block);
#if ENABLE_STATISTIC
                auto endTime = std::chrono::high_resolution_clock::now();
                m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                m_statistic.registerAllocation<0>(table._size);
#endif //ENABLE_STATISTIC
                return block->ptr();
            }

            Pool* pool = nullptr;// MemoryPool::allocatePool(&table, k_maxSizeTableAllocation, aligment);
            table._pools.push_back(pool);

            /*block = pool->_free.back();
            pool->_free.pop_back();*/

            pool->_used.insert(block);

#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<0>(table._size);
#endif //ENABLE_STATISTIC
            return block->ptr();
        }
        else if (aligmentSize < k_maxSizeTableAllocation)
        {
            //pool allocation
            PoolTable& table = m_poolTable;
            for (auto& pool : table._pools)
            {
                if (!pool->_free.empty())
                {
                    Block* block = pool->_free.begin();
                    while (block != pool->_free.end())
                    {
                        u64 requestedSize = aligmentSize + sizeof(Block);
                        if (block->_size >= requestedSize)
                        {
                            u64 freeMemory = block->_size - requestedSize;
                            block->_size = requestedSize;

                            address_ptr emptyMemory = (address_ptr)(reinterpret_cast<u64>(block->ptr()) + requestedSize);
                            Block* emptyBlock = initBlock(emptyMemory, pool, freeMemory);
                            pool->_free.insert(emptyBlock);

                            pool->_free.erase(block);
                            pool->_used.insert(block);

#if ENABLE_STATISTIC
                            auto endTime = std::chrono::high_resolution_clock::now();
                            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

                            m_statistic.registerAllocation<1>(requestedSize);
#endif //ENABLE_STATISTIC

                            address_ptr ptr = block->ptr();
                            return ptr;
                        }
                        block = block->_next;
                    }
                }
            }

            //create new pool
            u64 allocationSize = alignUp<u64>(k_maxSizeTableAllocation / (m_maxSmallTableAllocation + sizeof(Block)), aligment);
            address_ptr memory = m_allocator->allocate(allocationSize, aligment, m_userData);

            Pool* pool = new Pool(&table, allocationSize);

            Block* block = initBlock(memory, pool, allocationSize);
            pool->_free.insert(block);

            u64 requestedSize = aligmentSize + sizeof(Block);
            if (block->_size > requestedSize)
            {
                u64 freeMemory = block->_size - requestedSize;
                block->_size = requestedSize;

                address_ptr emptyMemory = (address_ptr)(reinterpret_cast<u64>(memory) + requestedSize);
                Block* emptyBlock = initBlock(emptyMemory, pool, freeMemory);
                pool->_free.insert(emptyBlock);
            }

            pool->_free.erase(block);
            pool->_used.insert(block);
            table._pools.push_back(pool);

#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<1>(requestedSize);
            m_statistic.registerPoolAllcation<1>(allocationSize);
#endif //ENABLE_STATISTIC

            address_ptr ptr = block->ptr();
            return ptr;
        }
        else
        {
            //large allocation
            u64 allocationSize = alignUp<u64>(aligmentSize + sizeof(Block), aligment);
            address_ptr memory = m_allocator->allocate(allocationSize, aligment, m_userData);

            Block* block = initBlock(memory, nullptr, allocationSize);
            m_largeAllocations.insert(block);

#if ENABLE_STATISTIC
            auto endTime = std::chrono::high_resolution_clock::now();
            m_statistic._allocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

            m_statistic.registerAllocation<2>(allocationSize);
            m_statistic.registerPoolAllcation<2>(allocationSize);
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
            u64 blockSize = block->_size;

            Pool* pool = block->_pool;
            pool->_used.erase(block);

            block->reset();
            pool->_free.insert(block);
            pool->_free.merge();
#if ENABLE_STATISTIC
            m_statistic.registerDeallocation<1>(blockSize);
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
                for (auto iter = std::next(markTodelete.begin(), 1); iter != markTodelete.end(); ++iter)
                {
                    MemoryPool::deallocatePool(*iter);
#if ENABLE_STATISTIC
                    m_statistic.registerPoolDeallcation<1>((*iter)->_size);
#endif //ENABLE_STATISTIC
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
            m_statistic.registerDeallocation<2>(blockSize);
            m_statistic.registerPoolDeallcation<2>(blockSize);
#endif //ENABLE_STATISTIC
        }

#if ENABLE_STATISTIC
        auto endTime = std::chrono::high_resolution_clock::now();
        m_statistic._dealocateTime += std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
#endif //ENABLE_STATISTIC
    }

    void MemoryPool::clear()
    {
        //TODO
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

    MemoryPool::Pool* MemoryPool::allocatePool(PoolTable* table, u64 tableSize, u64 size, u32 align)
    {
        Pool* pool = new Pool(table, size);

        u64 countAllocations = tableSize / sizeof(Block);
        address_ptr mem = m_allocator->allocate(countAllocations * (size + sizeof(Block)), align, m_userData);

        pool->_table = table;
        //pool->_free.resize(countAllocations, nullptr);

        u64 offset = 0;
//        for (auto iter = pool->_free.rbegin(); iter != pool->_free.rend(); ++iter)
//        {
//            Block*& block = (*iter);
//
//            address_ptr ptr = (address_ptr)(reinterpret_cast<u64>(mem) + offset);
//            block = new(ptr)Block();
//            block->_pool = pool;
//            block->_size = size;
//#if DEBUG_MEMORY
//            block->_ptr = (address_ptr)(reinterpret_cast<u64>(ptr) + sizeof(Block));
//#endif //DEBUG_MEMORY
//            offset += size + sizeof(Block);
//        }

        return pool;
    }

    void MemoryPool::deallocatePool(Pool* pool)
    {
        //TODO
    }

    MemoryPool::Block* MemoryPool::initBlock(address_ptr ptr, Pool* pool, u64 size)
    {
        Block* block = new(ptr)Block();
#if DEBUG_MEMORY
        block->reset();
        block->_ptr = (address_ptr)(reinterpret_cast<u64>(ptr) + sizeof(Block));
#endif //DEBUG_MEMORY
        block->_size = size;
        block->_pool = pool;

        return block;
    }

#ifdef ENABLE_STATISTIC
    void MemoryPool::collectStatistic()
    {
        std::cout << "Pool Statistic" << std::endl;
        std::cout << "Time alloc/dealloc (ms): " << (double)m_statistic._allocateTime / 1000.0 << "/" << (double)m_statistic._dealocateTime / 1000.0 << std::endl;
        std::cout << "Count Allocation : " << m_statistic._generalAllocationCount << ". Size (b): " << m_statistic._generalAllocationSize << std::endl;
        std::cout << "Pool Staticstic:" << std::endl;
        std::cout << " SmallTable - Sizes/PoolSizes (b): " << m_statistic._tableAllocationSizes[0] << "/" << m_statistic._poolAllocationSizes[0] 
            << " Count Allcations/Pools: " << m_statistic._tableAllocationCount[0] << "/" << m_statistic._poolsAllocationCount[0] << std::endl;
        std::cout << " PoolTable - Sizes/PoolSizes (b): " << m_statistic._tableAllocationSizes[1] << "/" << m_statistic._poolAllocationSizes[1]
            << " Count Allcations/Pools: " << m_statistic._tableAllocationCount[1] << "/" << m_statistic._poolsAllocationCount[1] << std::endl;
        std::cout << " LargeAllocations - Sizes/PoolSizes (b): " << m_statistic._tableAllocationSizes[2] << "/" << m_statistic._poolAllocationSizes[2]
            << " Count Allcations/Pools: " << m_statistic._tableAllocationCount[2] << "/" << m_statistic._poolsAllocationCount[2] << std::endl;
    }
#endif //ENABLE_STATISTIC

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
