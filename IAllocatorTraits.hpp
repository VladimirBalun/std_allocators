namespace std_allocators
{

	template<typename T>
	class LinearAllocator;

	template<typename T>
	class StackAllocator;

	template<typename T>
	class PooolAllocator;

	namespace details
    { 

		template<typename T>
		struct BaseAllocatorTraits
		{
			using value_type      = T;
			using pointer         = T*;
			using const_pointer   = const T*;
			using reference       = T&;
            using const_reference = const T&;
		};

        template<typename T>
		struct IAllocatorTraits<LinearAllocator<T>> : BaseAllocatorTraits<T>
		{
			static typename BaseAllocatorTraits<T>::pointer callAllocateImpl(LinearAllocator<T>* object, size_t count_objects)
			{
            	return object->allocateImpl(count_objects);
			}

			static void callDeallocateImpl(LinearAllocator<T>* object, typename BaseAllocatorTraits<T>::pointer memory_pointer, size_t count_objects)
			{
				object->deallocateImpl(memory_pointer, count_objects);
			}
		};

        // template<typename T>
		// struct IAllocatorTraits<StackAllocator<T>> : BaseAllocatorTraits<T>
		// {
        // 
		//  };

        // template<typename T>
		// struct IAllocatorTraits<PoolAllocator<T>> : BaseAllocatorTraits<T>
		// {
        // 
		// };

	}

}
