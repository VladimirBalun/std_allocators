#include <vector>
#include <deque>
#include <iostream>
#include <array>
#include <algorithm>
#include <set>
#include <list>
#include <map>
#include <utility>

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

    template<std::size_t CHUNK_SIZE, class Container>
    class Chunk
    {
        static constexpr std::size_t HEADER_SIZE = 4u;
    public:
        Chunk()
        {
            std::uint32_t* init_header = reinterpret_cast<std::uint32_t*>(m_blocks.data.data());
            *init_header = m_max_block_size;
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
            if (allocation_size > m_max_block_size)
            {
                return nullptr;
            }
            
            const auto it = std::min_element(m_free_blocks.cbegin(), m_free_blocks.cend(), [] (std::uint32_t* lhs, std::uint32_t* rhs)
            {
                return (*lhs) < (*rhs);
            });
            
            assert(it != m_free_blocks.cend() && "Internal logic error with reserve block");
            
            std::uint32_t* header_address = *it;
            std::uint32_t* new_header_address =
                reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(header_address) + HEADER_SIZE + allocation_size);
            if (m_free_blocks.find(new_header_address) == m_free_blocks.cend())
            {
                const std::uint32_t old_block_size = *header_address;
                const std::uint32_t new_block_size = old_block_size - static_cast<std::uint32_t>(allocation_size) - HEADER_SIZE; // TODO
                *new_header_address = new_block_size;
                m_free_blocks.insert(new_header_address);
            }
            
            m_free_blocks.erase(header_address);
            *header_address = static_cast<std::uint32_t>(allocation_size);
            std::uint8_t* allocated_block = reinterpret_cast<std::uint8_t*>(header_address) + HEADER_SIZE;
            return allocated_block;
        }
        
        void relizeBlock(std::uint8_t* block_ptr)
        {
            std::uint8_t* header_address = block_ptr - HEADER_SIZE;
            m_free_blocks.insert(reinterpret_cast<std::uint32_t*>(header_address));
        }
        
        void defragment()
        {
            if (m_free_blocks.size() < 2u)
            {
                return;
            }
            
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
                        *prev_header_address = prev_block_size + HEADER_SIZE + current_block_size;
                        it = m_free_blocks.erase(it);
                        continue;
                    }
                }
                
                prev_header_address = current_header_address;
            }
        }
    private:
        Container m_blocks{};
        std::set<std::uint32_t*> m_free_blocks{};
        std::uint32_t m_max_block_size = CHUNK_SIZE - HEADER_SIZE;
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
        : m_allocation_strategy(other.m_allocation_strategy)
    {
        static_assert(std::is_convertible_v<T*, U*>, "Can not copy allocators: types are not convertible");
    }
    
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
    AllocationStrategy* m_allocation_strategy = nullptr;
};

template<typename T>
using CustomAllocator = Allocator<T, CustomAllocationStrategy<details::StackChunk>>;

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
            delete_custom_unique<T> custom_deleter{ m_allocator };
            return custom_unique_ptr<T>{ new (memory_block) T{ std::forward<Args>()... }, custom_deleter };
        }

        return nullptr;
    }
private:
    Allocator* m_allocator;
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
        ptr->~T();
        if (m_allocator)
        {
            m_allocator->deallocate(ptr, 1u);
        }
    }
private:
    Allocator* m_allocator;
};

int main(int argc, char** argv)
{
    CustomAllocationStrategy<details::StackChunk> allocationStrategy{};
    CustomAllocator<int> custom_allocator{ allocationStrategy };
    
    /*custom_vector<int> vector{ custom_allocator };
    for (int i = 0u; i < 1'000; ++i)
    {
        vector.push_back(i);
        std::cout << vector.at(i) << " ";
    }
    vector.resize(16u);*/
    
    {
        custom_unique_ptr<int> ptr = make_custom_unique<int>(custom_allocator)();
    }
    
    
    return EXIT_SUCCESS;
}

