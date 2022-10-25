#pragma once

#include "Utility/UtilityMacros.h"
#include "SculptorAliases.h"

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
		, m_prevAllocationBegin(nullptr)
		, m_prevAllocationEnd(nullptr)
	{ }

	explicit InlineAllocator(Byte* storagePtr, SizeType storageSize)
		: m_inlineStoragePtr(storagePtr)
		, m_inlineStorageSize(storageSize)
		, m_prevAllocationBegin(storagePtr)
		, m_prevAllocationEnd(storagePtr)
	{
		SPT_CHECK(m_inlineStoragePtr != nullptr && m_inlineStorageSize > 0);
	}

	template <class TOther>
	explicit InlineAllocator(const InlineAllocator<TOther, TFallbackAllocator>& other)
		: m_inlineStoragePtr(other.GetInlineStoragePtr())
		, m_inlineStorageSize(other.GetInlineStorageSize())
		, m_prevAllocationBegin(other.GetPrevAllocationBegin())
		, m_prevAllocationEnd(other.GetPrevAllocationEnd())
	{ }

	SPT_NODISCARD constexpr TType* allocate(std::size_t n)
	{
		SPT_PROFILER_FUNCTION();
		
		SPT_CHECK(m_prevAllocationEnd != nullptr && m_prevAllocationBegin != nullptr && m_inlineStoragePtr != nullptr && m_inlineStorageSize > 0);

		std::size_t allocationSize = n * sizeof(TType);

		if (allocationSize < m_inlineStorageSize)
		{
			if (static_cast<std::size_t>(m_prevAllocationBegin - m_inlineStoragePtr) >= allocationSize)
			{
				m_prevAllocationBegin = m_inlineStoragePtr;
				m_prevAllocationEnd = m_inlineStoragePtr + allocationSize;
				return reinterpret_cast<TType*>(m_inlineStoragePtr);
			}
			else if (static_cast<std::size_t>((m_inlineStoragePtr + m_inlineStorageSize) - m_prevAllocationEnd) >= allocationSize)
			{
				m_prevAllocationBegin = m_prevAllocationEnd;
				m_prevAllocationEnd = m_prevAllocationBegin + allocationSize;
				return reinterpret_cast<TType*>(m_prevAllocationBegin);
			}
		}

		m_prevAllocationBegin = m_prevAllocationEnd = m_inlineStoragePtr;

		return m_fallbackAllocator.allocate(n);
	}

	constexpr void deallocate(TType* ptr, std::size_t n)
	{
		SPT_PROFILER_FUNCTION();

		if (reinterpret_cast<Byte*>(ptr) > m_inlineStoragePtr && reinterpret_cast<Byte*>(ptr) < (m_inlineStoragePtr + m_inlineStorageSize))
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

	Byte* GetInlineStoragePtr() const
	{
		return m_inlineStoragePtr;
	}

	std::size_t GetInlineStorageSize() const
	{
		return m_inlineStorageSize;
	}

	Byte* GetPrevAllocationBegin() const
	{
		return m_prevAllocationBegin;
	}

	Byte* GetPrevAllocationEnd() const
	{
		return m_prevAllocationEnd;
	}

private:

	Byte*						m_inlineStoragePtr;
	std::size_t					m_inlineStorageSize;

	Byte*						m_prevAllocationBegin;
	Byte*						m_prevAllocationEnd;

	TFallbackAllocator<TType>	m_fallbackAllocator;
};

} // impl

template<typename TType, template<typename TFallbackType> typename TFallbackAllocator = std::allocator>
using InlineAllocator = impl::InlineAllocator<TType, TFallbackAllocator>;

} // spt::lib