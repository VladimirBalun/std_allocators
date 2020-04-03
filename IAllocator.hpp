namespace std_allocators
{

	namespace details
    { 

		template<typename Derived>
		struct IAllocatorTraits;

	}

	template<class Derived>
	struct IAllocator
	{
		using value_type    = typename details::IAllocatorTraits<Derived>::value_type;
		using pointer       = typename details::IAllocatorTraits<Derived>::pointer;
		using const_pointer = typename details::IAllocatorTraits<Derived>::const_pointer;

        pointer allocate(size_t count_objects);
		void deallocate(pointer memory_pointer, size_t count_objects);
	};

	template<class Derived>
    typename IAllocator<Derived>::pointer IAllocator<Derived>::allocate(size_t count_objects)
	{
		Derived* derived = static_cast<Derived*>(this);
		return details::IAllocatorTraits<Derived>::callAllocateImpl(derived, count_objects);
	}

	template<class Derived>
	void IAllocator<Derived>::deallocate(typename IAllocator<Derived>::pointer memory_pointer, size_t count_objects)
	{
		Derived* derived = static_cast<Derived*>(this);
		return details::IAllocatorTraits<Derived>::callDeallocateImpl(derived, memory_pointer, count_objects);
	}

}

