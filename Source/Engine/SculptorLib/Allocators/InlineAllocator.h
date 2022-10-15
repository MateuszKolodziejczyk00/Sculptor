#pragma once

#include "Utility/UtilityMacros.h"
#include "MathCore.h"

#include <memory.h>


namespace spt::lib
{

namespace impl
{

template<typename TType, template<typename TFallbackType> typename TFallbackAllocator = std::allocator> 
class InlineAllocator
{
public:

	using ThisType = InlineAllocator<TType, TFallbackAllocator>;
	
	using FallbackAllocatorTraits = std::allocator_traits<TFallbackAllocator<TType>>;

	using value_type = typename FallbackAllocatorTraits::value_type;
	using pointer = typename FallbackAllocatorTraits::pointer;
	using const_pointer = typename FallbackAllocatorTraits::const_pointer;
	using size_type = typename FallbackAllocatorTraits::size_type;
	using difference_type = typename FallbackAllocatorTraits::difference_type;

	template <class TOther>
	struct rebind { using other = InlineAllocator<TOther, TFallbackAllocator>; };

	InlineAllocator()
		: m_inlineStoragePtr(nullptr)
		, m_inlineStorageSize(0)
	{ }

	explicit InlineAllocator(void* storagePtr, SizeType storageSize)
		: m_inlineStoragePtr(storagePtr)
		, m_inlineStorageSize(storageSize)
	{
		SPT_CHECK(m_inlineStoragePtr != nullptr && m_inlineStorageSize > 0);
	}

	template <class TOther>
	explicit InlineAllocator(const InlineAllocator<TOther, TFallbackAllocator>& other)
		: m_inlineStoragePtr(other.GetInlineStoragePtr())
		, m_inlineStorageSize(other.GetInlineStorageSize())
	{ }

	SPT_NODISCARD constexpr TType* allocate(std::size_t n)
	{
		SPT_PROFILER_FUNCTION();
		
		SPT_CHECK(m_inlineStoragePtr != nullptr && m_inlineStorageSize > 0);

		return n * sizeof(TType) <= m_inlineStorageSize ? reinterpret_cast<TType*>(m_inlineStoragePtr) : m_fallbackAllocator.allocate(n);
	}

	constexpr void deallocate(TType* ptr, std::size_t n)
	{
		SPT_PROFILER_FUNCTION();

		if (n * sizeof(TType) > m_inlineStorageSize)
		{
			m_fallbackAllocator.deallocate(ptr, n);
		}
	}

	constexpr Bool operator==(const ThisType& rhs) const noexcept
    {
		return true;
    }

    constexpr Bool operator!=(const ThisType& rhs) const noexcept
    {
        return !(*this == rhs);
    }

	void* GetInlineStoragePtr() const
	{
		return m_inlineStoragePtr;
	}

	std::size_t GetInlineStorageSize() const
	{
		return m_inlineStorageSize;
	}

private:

	void*						m_inlineStoragePtr;
	std::size_t					m_inlineStorageSize;
	TFallbackAllocator<TType>	m_fallbackAllocator;
};

} // impl

template<typename TType, template<typename TFallbackType> typename TFallbackAllocator = std::allocator>
using InlineAllocator = impl::InlineAllocator<TType, TFallbackAllocator>;

} // spt::lib