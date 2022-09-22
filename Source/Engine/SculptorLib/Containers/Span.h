#pragma once

#include <span>


namespace spt::lib
{

template <class TElementType, SizeType extent = std::dynamic_extent>
using Span = std::span<TElementType, extent>;

} // spt::lib
