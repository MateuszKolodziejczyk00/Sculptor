#pragma once

#include <cstdint>
#include <cstddef>

#include <limits>


namespace spt
{

using Int8 = int8_t;
using Uint8 = uint8_t;

using Byte = std::byte;

using Int16 = int16_t;
using Uint16 = uint16_t;

using Int32 = int32_t;
using Uint32 = uint32_t;

using Int64 = int64_t;
using Uint64 = uint64_t;

using SizeType = size_t;

using IntPtr = intptr_t;

using Flags8 = Uint8;
using Flags16 = Uint16;
using Flags32 = Uint32;
using Flags64 = Uint64;

using Bool = bool;

using Real32 = float;
using Real64 = double;

template<typename TType>
static constexpr TType idxNone = static_cast<TType>(-1);

template<typename TType>
static constexpr TType minValue = std::numeric_limits<TType>::min();

template<typename TType>
static constexpr TType maxValue = std::numeric_limits<TType>::max();

template<typename TType>
static constexpr TType pi = static_cast<TType>(3.14159265358979323846);

} // spt
