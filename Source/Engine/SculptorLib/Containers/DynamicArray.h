#pragma once

#include <vector>


namespace spt::lib
{

template <class TType, class TAllocator = std::allocator<TType>>
using DynamicArray = std::vector<TType, TAllocator>;

} // spt::lib