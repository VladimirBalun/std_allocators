#include <memory>

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
        // Necessary traits for std::allocator
        using size_type        = std::size_t;
        using difference_type  = std::ptrdiff_t;
        using value_type       = typename details::IAllocatorTraits<Derived>::value_type;
        using pointer          = typename details::IAllocatorTraits<Derived>::pointer;
        using const_pointer    = typename details::IAllocatorTraits<Derived>::const_pointer;

        // Unnecessary traits for std::allocator
        using allocator_traits = std::allocator_traits<Derived>;
        using reference        = typename details::IAllocatorTraits<Derived>::reference;
        using const_reference  = typename details::IAllocatorTraits<Derived>::const_reference;

        pointer allocate(std::size_t count_objects);
        void deallocate(pointer memory_pointer, std::size_t count_objects);
    };

    template<class Derived>
    typename IAllocator<Derived>::pointer IAllocator<Derived>::allocate(std::size_t count_objects)
    {
        Derived* derived = static_cast<Derived*>(this);
        return details::IAllocatorTraits<Derived>::callAllocateImpl(derived, count_objects);
    }

    template<class Derived>
    void IAllocator<Derived>::deallocate(typename IAllocator<Derived>::pointer memory_pointer, std::size_t count_objects)
    {
        Derived* derived = static_cast<Derived*>(this);
        return details::IAllocatorTraits<Derived>::callDeallocateImpl(derived, memory_pointer, count_objects);
    }

    template<class LHSDerived, class RHSDerived>
    bool operator == (const IAllocator<LHSDerived>& lhs, const IAllocator<RHSDerived>& rhs)
    {
        const LHSDerived* lhs_derived = static_cast<const LHSDerived*>(&lhs);
        const RHSDerived* rhs_derived = static_cast<const RHSDerived*>(&rhs);
        return lhs_derived == rhs_derived;
    }

    template<class LHSDerived, class RHSDerived>
    bool operator != (const IAllocator<LHSDerived>& lhs, const IAllocator<RHSDerived>& rhs)
    {
        return !(lhs == rhs);
    }

}

