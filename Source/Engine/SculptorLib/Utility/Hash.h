#pragma once

#include "MathCore.h"

#include <numeric>
#include <functional>


namespace spt::lib
{

namespace priv
{

template<typename TType>
struct IsHashableHelper
{
private:

	template<typename T>
	static Uint64		IsHashableImpl(decltype(std::declval<std::hash<T>>()(std::declval<T>())));

	template<typename T>
	static Uint32		IsHashableImpl(...);

public:

	enum
	{
		value = sizeof(IsHashableImpl<TType>(0)) == sizeof(Uint64)
	};
};

}


template<typename TType>
static constexpr Bool isHashable = priv::IsHashableHelper<TType>::value;


template<typename Iterator>
constexpr SizeType HashRange(const Iterator begin, const Iterator end)
{
	using ElementType = Iterator::value_type;

	// Source: Thomas Mueller's post https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key/12996028#12996028
	const auto elementHash = [](SizeType seed, const ElementType& element) -> SizeType
	{
		static_assert(isHashable<ElementType>, "Type must be hashable");
		const SizeType val = std::hash<ElementType>{}(element);

		SizeType x = val;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = (x >> 16) ^ x;
		return x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	};

	const SizeType length = std::distance(begin, end);
	return std::accumulate(begin, end, length, elementHash);
}

// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
template<typename THash>
inline void HashCombine(THash& seed, THash hash)
{
	constexpr THash magicNumber = 0x9e3889b9;
	seed ^= hash + magicNumber + (seed << 6) + (seed >> 2);
}

template<typename TType>
inline SizeType GetHash(const TType& value)
{
	std::hash<TType> hasher;
	return hasher(value);
}

}