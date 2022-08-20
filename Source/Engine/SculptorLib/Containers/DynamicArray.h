#pragma once

#include <vector>


namespace spt::lib
{

template <class Type, class Allocator = std::allocator<Type>>
using DynamicArray = std::vector<Type, Allocator>;

}