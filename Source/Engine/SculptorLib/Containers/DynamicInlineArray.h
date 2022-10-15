#pragma once

#include "DynamicArray.h"
#include "StaticArray.h"
#include "Allocators/InlineAllocator.h"


namespace spt::lib
{

template <typename TType, SizeType inlineSize, template<typename TFallbackType> typename TFallbackAllocator = std::allocator>
class DynamicInlineArray : public DynamicArray<TType, lib::InlineAllocator<TType, TFallbackAllocator>>
{
protected:

	using AllocatorType = lib::InlineAllocator<TType, TFallbackAllocator>;
	using Super = DynamicArray<TType, AllocatorType>;

public:

	using ThisType = DynamicInlineArray<TType, inlineSize, TFallbackAllocator>;

	constexpr DynamicInlineArray()
		: Super(AllocatorType(m_inlineStorage.data(), m_inlineStorage.size() * sizeof(TType)))
	{ }

	constexpr DynamicInlineArray(const DynamicInlineArray& other)
		: Super(std::cbegin(other), std::cend(other), AllocatorType(m_inlineStorage.data(), m_inlineStorage.size() * sizeof(TType)))
	{ }

	constexpr DynamicInlineArray(DynamicInlineArray&& other)
		: Super(AllocatorType(m_inlineStorage.data(), m_inlineStorage.size() * sizeof(TType)))
	{
		Super::operator=(std::forward<DynamicInlineArray>(other));
	}

	explicit constexpr DynamicInlineArray(const lib::DynamicArray<TType, TFallbackAllocator<TType>>& other)
		: Super(AllocatorType(m_inlineStorage.data(), m_inlineStorage.size() * sizeof(TType)))
	{
		*this = other;
	}

	DynamicInlineArray& operator=(const DynamicInlineArray& rhs)
	{
		Super::operator=(rhs);
		return *this;
	}

	DynamicInlineArray& operator=(DynamicInlineArray&& rhs)
	{
		Super::operator=(std::forward<DynamicInlineArray>(rhs));
		return *this;
	}
	
	DynamicInlineArray& operator=(const lib::DynamicArray<TType, TFallbackAllocator<TType>>& rhs)
	{
		Super::resize(rhs.size());
		std::copy(std::cbegin(rhs), std::cend(rhs), std::begin(*this));
		return *this;
	}

private:

	StaticArray<TType, inlineSize> m_inlineStorage;
};

} // spt::lib
