#pragma once

#include "MathCore.h"


namespace spt::math
{

struct Utils
{
public:

	template<typename TType> requires std::is_integral_v<TType>
	static constexpr TType RoundUp(TType value, TType multiple)
	{
		SPT_CHECK(multiple > 0);
		return ((value + multiple - 1) / multiple) * multiple;
	}

	template<typename TType> requires std::is_integral_v<TType>
	static constexpr Bool IsPowerOf2(TType value)
	{
		return (value & (value - 1)) == 0;
	}

	template<typename TType> requires std::is_integral_v<TType>
	static constexpr TType RoundUpToPowerOf2(TType value)
	{
		SPT_CHECK(value > 0);

		--value;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		value |= value >> 16;
		if constexpr (sizeof(TType) > 32)
		{
			value |= value >> 32;
		}
		if constexpr (sizeof(TType) > 64)
		{
			value |= value >> 64;
		}
		++value;

		return value;
	}

};

} // spt::math