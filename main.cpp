#include <vector>
#include <deque>
#include <iostream>
#include <array>
#include <algorithm>
#include <set>

namespace details
{

    template<std::size_t CHUNK_SIZE>
    struct StackBlockContainer
    {
        std::array<std::uint8_t, CHUNK_SIZE> data{};
    };

    template<std::size_t CHUNK_SIZE>
    struct HeapBlockContainer
    {
        std::vector<std::uint8_t> data{};
    };

    template<std::size_t CHUNK_SIZE, typename ContainerType>
    class Chunk
    {
        static constexpr std::size_t HEADER_SIZE = 4u;
    public:
        Chunk()
        {
            std::uint32_t* init_header = reinterpret_cast<std::uint32_t*>(m_blocks.data.data());
            *init_header = m_max_block_size;
            m_free_blocks.insert(reinterpret_cast<std::uint32_t*>(m_blocks.data.data()));
        }
        
        bool isInside(const std::uint8_t* address) const
        {
            const std::uint8_t* start_chunk_address = m_blocks.data.data();
            const std::uint8_t* end_chunk_address = start_chunk_address + CHUNK_SIZE;
            return (start_chunk_address >= address) && (address < end_chunk_address);
        }
        
        std::uint8_t* tryReserveBlock(std::size_t allocation_size)
        {
            if (allocation_size > m_max_block_size)
            {
                return nullptr;
            }
            
            assert(!m_free_blocks.empty() && "Internal logic error with reserve blocks");
            const auto it = std::min_element(m_free_blocks.cbegin(), m_free_blocks.cend(), [] (std::uint32_t* lhs, std::uint32_t* rhs)
            {
                return (*lhs) < (*rhs);
            });
            
            std::uint32_t* header_address = *it;
            const std::uint32_t old_block_size = *header_address;
            *header_address = static_cast<std::uint32_t>(allocation_size);
            std::uint32_t* new_header_address = header_address + allocation_size;
            if (m_free_blocks.find(new_header_address) == m_free_blocks.cend())
            {
                *new_header_address = old_block_size - static_cast<std::uint32_t>(allocation_size) - HEADER_SIZE; // TODO
                m_free_blocks.insert(new_header_address);
            }
            
            return reinterpret_cast<std::uint8_t*>(header_address + 1u);
        }
        
        void relizeBlock(std::uint8_t* block_ptr)
        {
            m_free_blocks.insert(reinterpret_cast<std::uint32_t*>(block_ptr));
        }
        
        void defragment()
        {
            if (m_free_blocks.size() > 1u)
            {
                auto next_it = m_free_blocks.cbegin();
                auto current_it = next_it++;
                while (next_it != m_free_blocks.cend())
                {
                    std::uint32_t* current_header_address = *current_it;
                    const std::uint32_t current_block_size = *current_header_address;
                    const std::uint32_t* next_header_address = *next_it;
                    if (current_header_address + current_block_size == next_header_address)
                    {
                        const std::uint32_t next_block_size = *next_header_address;
                        *current_header_address = current_block_size + next_block_size + HEADER_SIZE;
                        auto temp_it = next_it;
                        ++next_it;
                        m_free_blocks.erase(temp_it);
                        continue;
                    }
                    
                    ++current_it;
                    ++next_it;
                }
            }
        }
    private:
        ContainerType m_blocks{};
        std::set<std::uint32_t*> m_free_blocks{};
        std::uint32_t m_max_block_size = CHUNK_SIZE - HEADER_SIZE;
    };

    template<std::size_t CHUNK_SIZE>
    using StackChunk = Chunk<CHUNK_SIZE, StackBlockContainer<CHUNK_SIZE>>;

    template<std::size_t CHUNK_SIZE>
    using HeapChunk = Chunk<CHUNK_SIZE, HeapBlockContainer<CHUNK_SIZE>>;

 }

template<template<size_t> class ChunkType, std::size_t CHUNK_SIZE = 4096>
class CustomAllocationStrategy
{
    static_assert(CHUNK_SIZE != 0u, "Chunk size must be more, than zero");
    static_assert(CHUNK_SIZE <= std::numeric_limits<std::uint32_t>::max(),
        "Chunk size must be less or equal max value of the uint32_t");
public:
    void* allocate(std::size_t size)
    {
        assert(size <= CHUNK_SIZE && "Incorrect chunk size for future usage");

        if (size == 0u)
        {
            return nullptr;
        }
        
        for (auto& chunk : m_chunks)
        {
            std::uint8_t* allocated_block = chunk.tryReserveBlock(size);
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
        for (auto& chunk : m_chunks)
        {
            if (chunk.isInside(deallocation_ptr))
            {
                chunk.relizeBlock(deallocation_ptr);
                chunk.defragment();
            }
        }
    }
private:
    std::deque<ChunkType<CHUNK_SIZE>> m_chunks{ 1u };
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
    
    Allocator(const Allocator& other)
        : m_allocation_strategy(other.m_allocation_strategy) {}
    
    template<typename U>
    Allocator(const Allocator<U, AllocationStrategy>& other)
        : m_allocation_strategy(other.m_allocation_strategy)
    {
        static_assert(std::is_convertible_v<T*, U*>, "Can not copy allocators: types are not convretible");
    }
    
    T* allocate(std::size_t count_objects)
    {
        return static_cast<T*>(m_allocation_strategy.allocate(count_objects * sizeof(T)));
    }
    
    void deallocate(void* memory_ptr, std::size_t count_objects)
    {
        m_allocation_strategy.deallocate(memory_ptr, count_objects * sizeof(T));
    }
    
    template<typename U, typename... Args>
    T* construct(U* ptr, Args&&... args)
    {
        return nullptr;
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
    AllocationStrategy m_allocation_strategy{};
};

template<typename T>
using CustomAllocator = Allocator<T, CustomAllocationStrategy<details::StackChunk>>;

template<typename T>
using CustomAllocatorWithStackChunks = Allocator<T, CustomAllocationStrategy<details::StackChunk, 1'024u>>;

template<typename T>
using CustomAllocatorWithHeapChunks = Allocator<T, CustomAllocationStrategy<details::HeapChunk, 16'384u>>;

template<typename T>
using custom_vector = std::vector<T, CustomAllocator<T>>;

int main(int argc, char** argv)
{
    CustomAllocator<int> custom_allocator{};
    custom_vector<int> vector{ custom_allocator };
    for (int i = 0u; i < 1'000; ++i)
    {
        vector.push_back(i);
        std::cout << vector.at(i) << " ";
    }
    vector.resize(16u);
    return EXIT_SUCCESS;
}

