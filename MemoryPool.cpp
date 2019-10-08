#include "MemoryPool.h"

#include <memory>
#include <assert.h>

#define DUMP_MEMORY 0

namespace mem
{

MemoryPool::MemoryAllocator* MemoryPool::s_defaultMemoryAllocator = nullptr;

MemoryPool::MemoryPool(MemoryAllocator* allocator, void* user)
    : m_allocator(allocator)
    , m_userData(user)
{
}

MemoryPool::~MemoryPool()
{
    m_userData = nullptr;
}

address_ptr MemoryPool::allocMemory(u64 size, u32 aligment)
{
    //TDOO
    //check if can to set to small table use small table allocation
    //if medium size allocate page allocator
    //if huge allocate through malloc

    address_ptr ptr = m_allocator->allocate(size, aligment, m_userData);
    return ptr;
}

void MemoryPool::freeMemory(address_ptr memory)
{
    m_allocator->deallocate(memory, 0, m_userData);
}

void MemoryPool::clear()
{
}

MemoryPool::MemoryAllocator* MemoryPool::getDefaultMemoryAllocator()
{
    if (!s_defaultMemoryAllocator)
    {
        s_defaultMemoryAllocator = new DefaultMemoryAllocator();
    }
    return s_defaultMemoryAllocator;
}


address_ptr DefaultMemoryAllocator::allocate(u64 size, u32 aligment, void* user)
{
    address_ptr newBlock = malloc(size);
    assert(newBlock && "Invalid allocate");

#ifdef DEBUG
    memset(newBlock, 'X', size);
#endif //DEBUG
    return newBlock;
}

void DefaultMemoryAllocator::deallocate(address_ptr memory, u64 size, void* user)
{
    assert(memory && "Invalid block");
    free(memory);
}

} //namespace mem
