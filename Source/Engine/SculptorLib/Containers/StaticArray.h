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


template<typename T, SizeType Size>
struct StaticArrayTraits<StaticArray<T, Size>>
{
	using Type = T;
	static constexpr SizeType Size = Size;
};

} // spt::lib
