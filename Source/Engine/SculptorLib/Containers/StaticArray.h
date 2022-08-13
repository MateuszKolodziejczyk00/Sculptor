#pragma once

#include <array>


namespace spt::lib
{

template <class Type, SizeType Size>
using StaticArray = std::array<Type, Size>;

}
