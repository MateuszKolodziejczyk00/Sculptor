#pragma once

#include "MathCore.h"
#include "Containers/StaticArray.h"

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

} // priv


template<typename TType>
static constexpr Bool isHashable = priv::IsHashableHelper<TType>::value;


template<typename TIterator, typename THasher = std::hash<TIterator::value_type>>
constexpr SizeType HashRange(const TIterator begin, const TIterator end, THasher hasher = THasher{})
{
	using ElementType = TIterator::value_type;

	// Source: Thomas Mueller's post https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key/12996028#12996028
	const auto elementHash = [hasher](SizeType seed, const ElementType& element) -> SizeType
	{
		const SizeType val = hasher(element);

		SizeType x = val;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = (x >> 16) ^ x;
		return x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	};

	const SizeType length = std::distance(begin, end);
	return std::accumulate(begin, end, length, elementHash);
}


template<typename... TArgs>
constexpr SizeType HashCombine(TArgs... args)
{
	const auto getHash = []<typename TType>(const TType& val) -> SizeType
	{
		if constexpr (std::is_same_v<std::decay_t<TType>, SizeType>)
		{
			return val;
		}
		else
		{
			return GetHash(val);
		}
	};

	// double brackets are here to trigger underlying array aggreagate initialization 
	const lib::StaticArray<SizeType, sizeof...(TArgs)> argsView{ { getHash(args)... } };

	return HashRange(std::cbegin(argsView), std::cend(argsView));
}


// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
template<typename THash>
constexpr void HashCombine(THash& seed, THash hash)
{
	constexpr THash magicNumber = 0x9e3889b9;
	seed ^= hash + magicNumber + (seed << 6) + (seed >> 2);
}


template<typename TType>
constexpr SizeType GetHash(const TType& value)
{
	std::hash<TType> hasher;
	return hasher(value);
}

} // spt::lib