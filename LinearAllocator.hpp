#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#include "IAllocator.hpp"
#include "IAllocatorTraits.hpp"

namespace std_allocators
{

	template<typename T>
	class LinearAllocator : public IAllocator<LinearAllocator<T>>
	{
	public:
        using value_type    = typename details::IAllocatorTraits<T>::value_type;
		using pointer       = typename details::IAllocatorTraits<T>::pointer;
		using const_pointer = typename details::IAllocatorTraits<T>::const_pointer;
	public:
		LinearAllocator();
		LinearAllocator(const LinearAllocator& other);
		template<typename U>
		LinearAllocator(const LinearAllocator<U>& other);
		pointer allocateImpl(size_t count_objects);
		void deallocateImpl(pointer memory_pointer, size_t count_objects);
	};	

	template<typename T>
	LinearAllocator<T>::LinearAllocator()
	{

	}
	
	template<typename T>	
	LinearAllocator<T>::LinearAllocator(const LinearAllocator& other)
	{

	}
	
	template<typename T>	
	template<typename U>
	LinearAllocator<T>::LinearAllocator(const LinearAllocator<U>& other)
	{
	
	}

	template<typename T>
	typename LinearAllocator<T>::pointer LinearAllocator<T>::allocateImpl(size_t count_objects)
	{
		if (count_objects == 0u)
			return nullptr;

		return nullptr;
	}

	template<typename T>
	void LinearAllocator<T>::deallocateImpl(pointer memory_pointer, size_t count_objects)
	{
		assert("Linear allocator does not support this operation." \
			   "You can use another type of the allocator for these purposes.");
	}

}

template<typename T>
using LinearAllocator = std_allocators::IAllocator<std_allocators::LinearAllocator<T>>;

int main(int argc, char** argv)
{
	
	LinearAllocator<int> allocator;
	int* memory_block = allocator.allocate(1u);
	allocator.deallocate(memory_block, 1u);

	return EXIT_SUCCESS;  
}

