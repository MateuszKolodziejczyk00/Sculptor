#pragma once

#include <span>


namespace spt::lib
{

template <class TElementType, SizeType extent = std::dynamic_extent>
using ArrayView = std::span<TElementType, extent>;

} // spt::lib
