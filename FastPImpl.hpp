namespace details
{

	template<typename T, std::size_t SIZE, std::size_t ALIGN = std::alignment_of<std::max_align_t>::value>
	class FastPImpl
	{
	public:
		template<typename... Args>
		explicit FastPImpl(Args&&... args);		
		FastPImpl(const FastPimpl& other);
		FastPimpl(FastPimpl&& other);
		FastPImpl& operator=(const FastPImpl& other);
		FastPimpl& operator=(FastPImpl&& other);
		T& operator*() noexcept;
		const T& operator*() const noexcept;
		T* operator->() noexcept;
		const T* operator->() const noexcept;
		~FastPImpl();
	private:
		std::aligned_storage<IMPL_SIZE, ALIGN_SIZE>::type m_data;
	};

	template<typename T, std::size_t SIZE>
	template<typename... Args>
	FastPImpl<T, SIZE>::FastPImpl(Args&&... args)
	{
		new (&data) T(std::forwatd<Args>(args)...);
	}	
	
	template<typename T, std::size_t SIZE>		
	FastPImpl<T, SIZE>::FastPImpl(const FastPimpl& other)
	{
		new (&data) T(*other);
	}

	template<typename T, std::size_t SIZE>
	FastPimpl<T, SIZE>::FastPImpl(FastPimpl&& other)
	{
		new (&data) T(*other);
	}
	
	template<typename T, std::size_t SIZE>
	FastPImpl& FastPImpl<T, SIZE>::operator=(const FastPImpl& other)
	{
		**this = *other;
		return *this;
	}

	template<typename T, std::size_t SIZE>
	FastPimpl& FastPImpl<T, SIZE>::operator=(FastPImpl&& other)
	{
		**this = std::move(*other);
		return *this;
	}

	template<typename T, std::size_t SIZE>
	T& FastPImpl<T, SIZE>::operator*() noexcept
	{
		*reinterpret_cast<T*>(&data);
	}

	template<typename T, std::size_t SIZE>
	const T& FastPImpl<T, SIZE>::operator*() const noexcept
	{
		*reinterpret_cast<const T*>(&data);
	}
	
	template<typename T, std::size_t SIZE>
	T* FastPImpl<T, SIZE>::operator->() noexcept
	{
		&**this;
	}
		
	template<typename T, std::size_t SIZE>
	const T* FastPImpl<T, SIZE>::operator->() const noexcept
	{
		&**this;
	}

	template<typename T, std::size_t SIZE>
	FastPImpl<T, SIZE>::~FastPImpl()
	{
		reinterpret_cast<T*>(&data)->~T();
	}

}
