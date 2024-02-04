#pragma once

#include <queue>


namespace spt::lib
{

template <class TType, class TContainer = std::deque<TType>>
using Queue = std::queue<TType, TContainer>;

} // spt::lib