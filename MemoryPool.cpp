#include "MemoryPool.h"

#include <memory>
#include <assert.h>

#define DUMP_MEMORY 0

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
    { 16, 2 },
    { 24, 3 }
};

MemoryPool::MemoryAllocator* MemoryPool::s_defaultMemoryAllocator = nullptr;

MemoryPool::MemoryPool(u64 pageSize, MemoryAllocator* allocator, void* user)
    : m_allocator(allocator)
    , m_pageSize(pageSize)
    , m_userData(user)
{
    m_smallPoolTables.resize(k_sizesTable.size(), { {}, 0 });
    for (auto& [size, index] : k_sizesTable)
    {
        assert(index < m_smallPoolTables.size());
        m_smallPoolTables[index].init(size);
    }
}

MemoryPool::~MemoryPool()
{
    MemoryPool::clear();
    m_userData = nullptr;
}

address_ptr MemoryPool::allocMemory(u64 size, u32 aligment)
{
    if (aligment == 0) //default
    {
        aligment = 4;
    }

    u32 aligmentSize = alignUp<u32>(static_cast<u32>(size), aligment);
    if (aligmentSize <= m_smallPoolTables.back()._size)
    {
        //small allocations
        u32 index = k_sizesTable.at(aligmentSize);
        PoolTable& table = m_smallPoolTables[0];
        if (table._pools.empty())
        {
            Pool* pool = MemoryPool::allocatePool(&table, aligment);
            table._pools.push_back(pool);
        }

        Block* block = nullptr;
        for (auto& pool : table._pools)
        {
            if(pool->_free.empty())
            {
                continue;
            }

            block = pool->_free.back();
            pool->_free.pop_back();

            pool->_used.insert(block);

            return block->ptr();
        }

        Pool* pool = MemoryPool::allocatePool(&table, aligment);
        table._pools.push_back(pool);

        block = pool->_free.back();
        pool->_free.pop_back();

        pool->_used.insert(block);

        return block->ptr();
    }
    else if (false)
    {
        //medium allocation
        //TODO
    }
    else
    {
        //large allocation
        address_ptr ptr = m_allocator->allocate(aligmentSize + sizeof(Block), aligment, m_userData);

        Block* block = new(ptr)Block();
        block->reset();
        block->_pool = nullptr;
        block->_size = aligmentSize;
#if DEBUG_MEMORY
        block->_ptr = (address_ptr)(reinterpret_cast<u64>(ptr) + sizeof(Block));

        m_largeAllocations.insert(block);
#endif //DEBUG_MEMORY

        return block->ptr();
    }

    assert(false);
    return nullptr;
}

void MemoryPool::freeMemory(address_ptr memory)
{
    address_ptr ptr = (address_ptr)(reinterpret_cast<u64>(memory) - sizeof(Block));
    Block* block = (Block*)ptr;
    assert(block);
    if (block->_pool)
    {
        Pool* pool = block->_pool;
        pool->_used.erase(block);

        block->reset();
        pool->_free.push_back(block);

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
        m_allocator->deallocate(ptr, block->_size, m_userData);
    }
}

void MemoryPool::clear()
{
    //TODO
}

MemoryPool::MemoryAllocator* MemoryPool::getDefaultMemoryAllocator()
{
    if (!s_defaultMemoryAllocator)
    {
        s_defaultMemoryAllocator = new DefaultMemoryAllocator();
    }
    return s_defaultMemoryAllocator;
}

MemoryPool::Pool* MemoryPool::allocatePool(PoolTable* table, u32 align)
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

void MemoryPool::deallocatePool(Pool* pool)
{
    //TODO
}

#ifdef ENABLE_STATISTIC
void MemoryPool::collectStatistic()
{
    //TODO
}
#endif //ENABLE_STATISTIC

address_ptr DefaultMemoryAllocator::allocate(u64 size, u32 aligment, void* user)
{
    address_ptr ptr = malloc(size);
    assert(ptr && "Invalid allocate");

#ifdef DEBUG_MEMORY
    memset(ptr, 'X', size);
#endif //DEBUG_MEMORY
    return ptr;
}

void DefaultMemoryAllocator::deallocate(address_ptr memory, u64 size, void* user)
{
    assert(memory && "Invalid block");
    free(memory);
}

void MemoryPool::PoolTable::init(u32 size)
{
    assert(_pools.empty());
    _size = size;
}

} //namespace mem
