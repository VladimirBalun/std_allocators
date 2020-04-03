namespace std_allocators
{

	template<typename T>
	class LinearAllocator;

	namespace details
    { 

        template<typename T>
		struct IAllocatorTraits<LinearAllocator<T>>
		{
			using value_type    = T;
			using pointer       = T*;
			using const_pointer = const T*;

			static pointer callAllocateImpl(LinearAllocator<T>* object, size_t count_objects)
			{
            	return object->allocateImpl(count_objects);
			}

			static void callDeallocateImpl(LinearAllocator<T>* object, pointer memory_pointer, size_t count_objects)
			{
				object->deallocateImpl(memory_pointer, count_objects);
			}
		};

	}

}
