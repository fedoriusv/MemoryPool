#pragma once

#include <assert.h>
#include <vector>
#include <array>
#include <list>
#include <map>

#define DEBUG_MEMORY 0
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
    typedef signed int          s32;
    typedef unsigned short      u16;
    typedef void*               address_ptr;

    /*
    * class MemoryPool
    */
    class MemoryPool
    {
    public:

        MemoryPool(const MemoryPool&) = delete;
        MemoryPool& operator=(const MemoryPool&) = delete;

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
        * MemoryPool constuctor
        * param pageSize : page size
        * param allocator: allocator
        * param user: user data
        */
        explicit MemoryPool(u64 pageSize, MemoryAllocator* allocator = MemoryPool::getDefaultMemoryAllocator(), void* user = nullptr);

        /*
        * ~MemoryPool destuctor
        */
        ~MemoryPool();

        /*
        * Request free memory from pool
        * param size: count bytes will be requested
        * param aligment: aligment
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
        void freeMemory(address_ptr memory);

        /*
        * Free pool, Return all requested allocation
        */
        void clear();

        /*
        * default allocator
        */
        static MemoryAllocator* getDefaultMemoryAllocator();

//#if ENABLE_STATISTIC
        void collectStatistic();
//#endif //ENABLE_STATISTIC

    private:

        MemoryAllocator*    m_allocator;
        void*               m_userData;

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
            inline address_ptr ptr()
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

            BlockList() noexcept
                : _end(nullptr)
                , _size(0)
            {
                clear();
            }

            ~BlockList()
            {
                if (_end)
                {
                    delete _end;
                    _end = nullptr;
                }
            }

            inline Block* begin()
            {
                return _end->_next;
            }

            inline Block* end()
            {
                return _end;
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

                _size = 0;
            }

            void priorityInsert(Block* block)
            {
                Block* current = begin();
                while (current != end())
                {
                    if (block < current)
                    {
                        link(current->_prev, block);
                        link(block, current);

                        ++_size;

                        return;
                    }
                    current = current->_next;
                }

                link(_end->_prev, block);
                link(block, _end);

                ++_size;
            }

            inline void insert(Block* block)
            {
                link(_end->_prev, block);
                link(block, _end);

                ++_size;
            }

            void merge()
            {
                if (empty())
                {
                    return;
                }

                Block* block = begin();
                Block* blockNext = begin()->_next;
                while (blockNext != end())
                {
                    address_ptr ptrNext = (address_ptr)(reinterpret_cast<u64>(block->ptr()) + block->_size);
                    if (blockNext == ptrNext)
                    {
                        block->_size += blockNext->_size;
                        blockNext = erase(blockNext);

                        continue;
                    }

                    block = block->_next;
                    blockNext = blockNext->_next;
                }
            }

            Block* erase(Block* block)
            {
                link(block->_prev, block->_next);

                --_size;
                assert(_size >= 0);

                return block->_next;
            }

            inline bool empty() const
            {
                return _end == _end->_next;
            }

        private:

            inline void link(Block* l, Block* r)
            {
                l->_next = r;
                r->_prev = l;
            }

            Block* _end;
            s32 _size;
        };

        struct Pool
        {
            Pool() = delete;
            Pool(const Pool&) = delete;

            Pool(PoolTable const* table, u64 blockSize, address_ptr memory, u64 poolSize) noexcept
                : _table(table)
                , _memory(memory)
                , _blockSize(blockSize)
                , _poolSize(poolSize)
            {
                _used.clear();
                _free.clear();
            }

            PoolTable const*    _table;
            address_ptr         _memory;
            const u64           _blockSize;
            const u64           _poolSize;
            BlockList           _used;
            BlockList           _free;
        };

        struct PoolTable
        {
            enum Type : u32
            {
                Default = 0,
                SmallTable,
            };

            PoolTable() noexcept
                : _size(0)
                , _type(Type::Default)
            {
            }

            PoolTable(u64 size, PoolTable::Type type) noexcept
                : _size(size)
                , _type(Type::Default)
            {
            }

            u64              _size;
            Type             _type;
            std::list<Pool*> _pools;
        };

        static const u64        k_maxSizeSmallTableAllocation = 32'768;
        std::array<u16, (k_maxSizeSmallTableAllocation >> 2)> m_smallTableIndex;
        std::vector<PoolTable>  m_smallPoolTables;

        PoolTable               m_poolTable;
        //best size 65KB
        const u64               k_maxSizeTableAllocation;

        BlockList               m_largeAllocations;

        static MemoryAllocator* s_defaultMemoryAllocator;

        std::vector<Pool*>      m_markedToDelete;

        Pool*   allocateFixedBlocksPool(PoolTable* table, u32 align);
        void    deallocatePool(Pool* pool);

        Block* initBlock(address_ptr ptr, Pool* pool, u64 size);

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

                _generalAllocationCount = 0;
                _generalAllocationSize = 0;

                for (u32 i = 0; i < 3; ++i)
                {
                    _tableAllocationCount[i] = 0;
                    _tableAllocationSizes[i] = 0;

                    _poolsAllocationCount[i] = 0;
                    _poolAllocationSizes[i] = 0;
                }
                _globalAllocationSize = 0;
            }

            template<u32 type>
            void registerAllocation(u64 size)
            {
                static_assert(type < 3);
                ++_tableAllocationCount[type];
                _tableAllocationSizes[type] += size;
                assert((u64)_tableAllocationCount[type] < (u64)std::numeric_limits<s64>().max());

                ++_generalAllocationCount;
                assert((u64)_generalAllocationCount < (u64)std::numeric_limits<s64>().max());
                _generalAllocationSize += size;
            }

            template<u32 type>
            void registerDeallocation(u64 size)
            {
                static_assert(type < 3);
                --_tableAllocationCount[type];
                _tableAllocationSizes[type] -= size;
                assert(_tableAllocationCount[type] >= 0 && _tableAllocationSizes[type] >= 0);

                --_generalAllocationCount;
                _generalAllocationSize -= size;
                assert(_generalAllocationCount >= 0 && _generalAllocationSize >= 0);
            }

            template<u32 type>
            void registerPoolAllcation(u64 size)
            {
                static_assert(type < 3);
                ++_poolsAllocationCount[type];
                _poolAllocationSizes[type] += size;
                assert((u64)_poolsAllocationCount[type] < (u64)std::numeric_limits<s64>().max() && (u64)_poolAllocationSizes[type] < (u64)std::numeric_limits<s64>().max());
            }

            template<u32 type>
            void registerPoolDeallcation(u64 size)
            {
                static_assert(type < 3);
                --_poolsAllocationCount[type];
                _poolAllocationSizes[type] -= size;
                assert(_poolsAllocationCount[type] >= 0 && _poolAllocationSizes[type] >= 0);
            }

            u64 _allocateTime;
            u64 _dealocateTime;

            s64 _tableAllocationCount[3];
            s64 _tableAllocationSizes[3];
            s64 _generalAllocationCount;
            u64 _generalAllocationSize;

            s64 _poolsAllocationCount[3];
            s64 _poolAllocationSizes[3];
            u64 _globalAllocationSize;

        };

        Statistic   m_statistic;
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
