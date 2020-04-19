namespace details
{

	template<std::size_t SIZE>
	struct FastPImpl
	{
		static constexpr std::size_t IMPL_SIZE = SIZE;
		static constexpr std::size_t ALIGN_SIZE = std::alignment_of<std::max_align_t>::value;
		
		std::aligned_storage<IMPL_SIZE, ALIGN_SIZE>::type impl;
	};

}
