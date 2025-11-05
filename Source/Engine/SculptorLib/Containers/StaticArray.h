#pragma once

#include <array>


namespace spt::lib
{

template <class Type, SizeType Size>
using StaticArray = std::array<Type, Size>;


template<typename TArrayType>
struct StaticArrayTraits
{
};


template<typename T, SizeType Num>
struct StaticArrayTraits<StaticArray<T, Num>>
{
	using Type = T;
	static constexpr SizeType Size = Num;
};

} // spt::lib
