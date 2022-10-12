#pragma once

#include "MathCore.h"


namespace spt::math
{

struct Utils
{
public:

	template<typename TType> requires std::is_integral_v<TType>
	static TType RoundUp(TType value, TType multiple)
	{
		SPT_CHECK(multiple > 0);
		return ((value + multiple - 1) / multiple) * multiple;
	}

};

} // spt::math