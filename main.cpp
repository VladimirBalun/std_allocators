#include <vector>
#include <deque>
#include <iostream>
#include <array>
#include <algorithm>
#include <set>
#include <list>
#include <map>
#include <utility>

// ----------------------------------------------------------------------------------------------------------
// ------------------------------- < allocator implementation later > ---------------------------------------
// ----------------------------------------------------------------------------------------------------------

namespace details
{

    // Strategy for allocation chunk data on the stack
    template<std::size_t CHUNK_SIZE>
    struct StackBlockContainer
    {
        std::array<std::uint8_t, CHUNK_SIZE> data{};
    };

    // Strategy for allocation chunk data in the heap
    template<std::size_t CHUNK_SIZE>
    struct HeapBlockContainer
    {
        std::vector<std::uint8_t> data{};
    };

    std::size_t getAlignmentPadding(std::size_t not_aligned_address, std::size_t alignment)
    {
        if ( (alignment != 0u) && (not_aligned_address % alignment != 0u) )
        {
            const std::size_t multiplier = (not_aligned_address / alignment) + 1u;
            const std::size_t aligned_address = multiplier * alignment;
            return aligned_address - not_aligned_address;
        }

        return 0u;
    }

    // Current chunk implementation works only with size
    // aligned by 4 bytes, because HEADER_SIZE now also 4 bytes.
    // You can modify it without problems for your purposes.

    template<std::size_t CHUNK_SIZE, class Container>
    class Chunk
    {
        static constexpr std::size_t HEADER_SIZE = 4u;
        static_assert(CHUNK_SIZE % HEADER_SIZE == 0, "CHUNK_SIZE must be multiple of the four");
    public:
        Chunk()
        {
            std::uint32_t* init_header = reinterpret_cast<std::uint32_t*>(m_blocks.data.data());
            *init_header = CHUNK_SIZE - HEADER_SIZE;
            m_max_block = init_header;
            m_free_blocks.insert(init_header);
        }
        
        bool isInside(const std::uint8_t* address) const noexcept
         {
            const std::uint8_t* start_chunk_address = m_blocks.data.data();
            const std::uint8_t* end_chunk_address = start_chunk_address + CHUNK_SIZE;
            return (start_chunk_address <= address) && (address < end_chunk_address);
        }
        
        std::uint8_t* tryReserveBlock(std::size_t allocation_size)
        {
            const std::size_t not_aligned_address = reinterpret_cast<std::size_t>(m_max_block) + allocation_size;
            const std::size_t alignment_padding = getAlignmentPadding(not_aligned_address, HEADER_SIZE);
            const std::uint32_t allocation_size_with_alignment = static_cast<std::uint32_t>(allocation_size + alignment_padding);
            if ( (!m_max_block) || (allocation_size_with_alignment > *m_max_block) ) // Check on enaught memory for allocation
            {
                return nullptr;
            }
            
            // Find min available by size memory block
            const auto min_it = std::min_element(m_free_blocks.cbegin(), m_free_blocks.cend(), [allocation_size] (const std::uint32_t* lhs, const std::uint32_t* rhs)
            {
                return ((*lhs) < (*rhs)) && (*lhs >= allocation_size);
            });
            
            assert(min_it != m_free_blocks.cend() && "Internal logic error with reserve block, something wrong in implementation...");
            
            std::uint32_t* header_address = *min_it;
            std::uint32_t* new_header_address =
                reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(header_address) + HEADER_SIZE + allocation_size_with_alignment);
            if (m_free_blocks.find(new_header_address) == m_free_blocks.cend()) // check if there is free memory in the current block
            {
                const std::uint32_t old_block_size = *header_address;
                const std::uint32_t difference = old_block_size - HEADER_SIZE;
                if (difference >= allocation_size_with_alignment) // check if there is enough space for another block
                {
                    const std::uint32_t new_block_size = difference - allocation_size_with_alignment;
                    *new_header_address = new_block_size;
                    m_free_blocks.insert(new_header_address);
                }
            }
            
            m_free_blocks.erase(header_address);
            *header_address = static_cast<std::uint32_t>(allocation_size);
            if (header_address == m_max_block) // if the maximum block were changed, then need to find the maximum block again
            {
                // Find max block by size
                const auto max_it = std::max_element(m_free_blocks.cbegin(), m_free_blocks.cend(), [] (const std::uint32_t* lhs, const std::uint32_t* rhs)
                {
                    return (*lhs) < (*rhs);
                });
                
                // If there are no free blocks, therefore the memory in this chunk is over
                m_max_block = (max_it != m_free_blocks.cend()) ? (*max_it) : (nullptr);
            }

            return reinterpret_cast<std::uint8_t*>(header_address) + HEADER_SIZE;
        }
        
        void relizeBlock(std::uint8_t* block_ptr)
        {
            std::uint8_t* header_address = block_ptr - HEADER_SIZE;
            const std::uint32_t size_relized_block = *header_address;
            if ( (!m_max_block) || (size_relized_block > *m_max_block) ) // if the relized block is greater than the maximum, then need to replace it
            {
                m_max_block = reinterpret_cast<std::uint32_t*>(header_address);
            }
                
            m_free_blocks.insert(reinterpret_cast<std::uint32_t*>(header_address));
        }
        
        void defragment()
        {
            std::uint32_t* prev_header_address = nullptr;
            for (auto it = m_free_blocks.begin(); it != m_free_blocks.end(); ++it)
            {
                std::uint32_t* current_header_address = *it;
                if (prev_header_address)
                {
                    const std::uint32_t prev_block_size = *prev_header_address;
                    const std::uint32_t* available_current_block_address =
                        reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(prev_header_address) + HEADER_SIZE + prev_block_size);
                    if (available_current_block_address == current_header_address)
                    {
                        const std::uint32_t current_block_size = *current_header_address;
                        const std::uint32_t new_prev_block_size = prev_block_size + HEADER_SIZE + current_block_size;
                        *prev_header_address = new_prev_block_size;
                        assert(m_max_block && "Internal logic error with chunk interaction, something wrong in implementation...");
                        if ( (!m_max_block) || (new_prev_block_size > *m_max_block) )
                        {
                            m_max_block = reinterpret_cast<std::uint32_t*>(prev_header_address);
                        }
                            
                        it = m_free_blocks.erase(it);
                        continue;
                    }
                }
                
                prev_header_address = current_header_address;
            }
        }
    public:
        Container m_blocks;
        std::set<std::uint32_t*> m_free_blocks;
        std::uint32_t* m_max_block;
    };

    template<std::size_t CHUNK_SIZE>
    using StackChunk = Chunk<CHUNK_SIZE, StackBlockContainer<CHUNK_SIZE>>;

    template<std::size_t CHUNK_SIZE>
    using HeapChunk = Chunk<CHUNK_SIZE, HeapBlockContainer<CHUNK_SIZE>>;

}

template<template<size_t> class Chunk, std::size_t CHUNK_SIZE = 16'384u>
class CustomAllocationStrategy
{
    static_assert(CHUNK_SIZE != 0u, "Chunk size must be more, than zero");
    static_assert(CHUNK_SIZE <= std::numeric_limits<std::uint32_t>::max(),
        "Chunk size must be less or equal max value of the uint32_t");
public:
    void* allocate(std::size_t size)
    {
        assert(size < CHUNK_SIZE && "Incorrect chunk size for future usage");

        if (size == 0u)
        {
            return nullptr;
        }
        
        for (auto& chunk : m_chunks)
        {
            void* allocated_block = chunk.tryReserveBlock(size);
            if (allocated_block)
            {
                return allocated_block;
            }
        }

        m_chunks.push_back({});
        auto& chunk = m_chunks.back();
        std::uint8_t* allocated_block = chunk.tryReserveBlock(size);
        assert(allocated_block && "Internal chunk error");
        return allocated_block;
    }

    void deallocate(void* memory_ptr, std::size_t size)
    {
        if ( (!memory_ptr) || (size == 0u) )
        {
            return;
        }

        std::uint8_t* deallocation_ptr = static_cast<std::uint8_t*>(memory_ptr);
        size_t index = 1u;
        for (auto& chunk : m_chunks)
        {
            if (chunk.isInside(deallocation_ptr))
            {
                chunk.relizeBlock(deallocation_ptr);
                chunk.defragment();
            }
            index++;
        }
    }
private:
    std::deque<Chunk<CHUNK_SIZE>> m_chunks{ 1u };
};

template<typename T, class AllocationStrategy>
class Allocator
{
    static_assert(!std::is_same_v<T, void>, "Type of the allocator can not be void");
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
public:
    template<class U>
    struct rebind
    {
        using other = Allocator<U, AllocationStrategy>;
    };
public:
    Allocator() = default;
    
    explicit Allocator(AllocationStrategy& strategy) noexcept
    : m_allocation_strategy(&strategy) {}
    
    Allocator(const Allocator& other) noexcept
        : m_allocation_strategy(other.m_allocation_strategy) {}
    
    template<typename U>
    Allocator(const Allocator<U, AllocationStrategy>& other) noexcept
        : m_allocation_strategy(other.m_allocation_strategy) {}
    
    T* allocate(std::size_t count_objects)
    {
        return static_cast<T*>(m_allocation_strategy->allocate(count_objects * sizeof(T)));
    }
    
    void deallocate(void* memory_ptr, std::size_t count_objects)
    {
        m_allocation_strategy->deallocate(memory_ptr, count_objects * sizeof(T));
    }
    
    template<typename U, typename... Args>
    T* construct(U* ptr, Args&&... args)
    {
        return new (ptr) T { std::forward<Args>(args)... };
    }
    
    template<typename U>
    void destroy(U* ptr)
    {
        if (ptr)
        {
            ptr->~T();
        }
    }
private:
    AllocationStrategy* m_allocation_strategy = nullptr;
};

// ----------------------------------------------------------------------------------------------------------
// -------------------------------------- < usage tools laler > ---------------------------------------------
// ----------------------------------------------------------------------------------------------------------

template<typename T>
using CustomAllocator = Allocator<T, CustomAllocationStrategy<details::StackChunk, 12>>;

template<typename T>
using CustomAllocatorWithStackChunks = Allocator<T, CustomAllocationStrategy<details::StackChunk, 1'024u>>;

template<typename T>
using CustomAllocatorWithHeapChunks = Allocator<T, CustomAllocationStrategy<details::HeapChunk, 16'384u>>;

template<typename T>
using custom_vector = std::vector<T, CustomAllocator<T>>;

template<typename T>
using custom_list = std::list<T, CustomAllocator<T>>;

template<typename T>
using custom_map = std::map<T, CustomAllocator<T>>;

template<typename T>
using custom_set = std::set<T, CustomAllocator<T>>;

template<class Allocator>
class custom_unique_ptr_creator;

template<class Allocator>
class custom_unique_ptr_deleter;

template<typename T>
using make_custom_unique = custom_unique_ptr_creator<CustomAllocator<T>>;

template<typename T>
using delete_custom_unique = custom_unique_ptr_deleter<CustomAllocator<T>>;

template<typename T>
using custom_unique_ptr = std::unique_ptr<T, delete_custom_unique<T>>;

template<class Allocator>
class custom_unique_ptr_creator
{
public:
    using T = typename Allocator::value_type;
    
    custom_unique_ptr_creator() = default;
    explicit custom_unique_ptr_creator(Allocator& allocator) noexcept
        : m_allocator(&allocator) {}
    
    template<typename... Args>
    custom_unique_ptr<T> operator()(Args&&... args)
    {
        if (m_allocator)
        {
            void* memory_block = m_allocator->allocate(1u);
            T* constructed_object = m_allocator->construct(memory_block, std::forward<Args>()...);
            return custom_unique_ptr<T>{ constructed_object, delete_custom_unique<T>{ m_allocator } };
        }

        return nullptr;
    }
private:
    Allocator* m_allocator = nullptr;
};

template<class Allocator>
class custom_unique_ptr_deleter
{
public:
    using T = typename Allocator::value_type;
    
    explicit custom_unique_ptr_deleter(Allocator* allocator = nullptr) noexcept
        : m_allocator(allocator) {}
    
    void operator()(T* ptr)
    {
        if (m_allocator)
        {
            m_allocator->destroy(ptr);
            m_allocator->deallocate(ptr, 1u);
        }
    }
private:
    Allocator* m_allocator;
};

// ----------------------------------------------------------------------------------------------------------
// -------------------------------- < usage example later > -------------------------------------------------
// ----------------------------------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    CustomAllocationStrategy<details::StackChunk, 12> allocationStrategy{};
    CustomAllocator<double> custom_allocator{ allocationStrategy };
    
    /*custom_vector<int> vector{ custom_allocator };
    for (int i = 0u; i < 1'000; ++i)
    {
        vector.push_back(i);
        std::cout << vector.at(i) << " ";
    }*/
    //vector.resize(16u);
    
    {
        custom_unique_ptr<double> ptr1 = make_custom_unique<double>(custom_allocator)();
        custom_unique_ptr<double> ptr2 = make_custom_unique<double>(custom_allocator)();
        custom_unique_ptr<double> ptr3 = make_custom_unique<double>(custom_allocator)();
        custom_unique_ptr<double> ptr4 = make_custom_unique<double>(custom_allocator)();
    }
    
    return EXIT_SUCCESS;
}
