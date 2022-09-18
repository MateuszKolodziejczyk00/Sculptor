#pragma once

#include "DynamicArray.h"

#include <stack>


namespace spt::lib
{

template<typename TElementType, typename TContainer = lib::DynamicArray<TElementType>>
using Stack = std::stack<TElementType, TContainer>;

} // spt::lib