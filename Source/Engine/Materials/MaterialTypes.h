#pragma once

#include "SculptorCoreTypes.h"


namespace spt::mat
{

namespace constants
{

constexpr Uint64 materialDataAlignmentBits = 5u;
constexpr Uint64 materialDataAlignment     = 1u << materialDataAlignmentBits;
constexpr Uint64 materialDataAlignmentMask = materialDataAlignment - 1u;

} // constants

} // namespace spt::mat
