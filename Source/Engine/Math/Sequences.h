#pragma once

#include "MathCore.h"
#include <concepts>


namespace spt::math
{

struct Sequences
{
public:

	// Based on https://en.wikipedia.org/wiki/Halton_sequence
	template<std::floating_point TType>
	static TType Halton(Uint32 idx, Uint32 base)
	{
		TType realBase = static_cast<TType>(base);

		Real32 f = 1.f;
		Real32 result = 0.f;

		while (idx > 0)
		{
			f = f / realBase;
			result = result + f * (idx % base);
			idx = idx / base;
		}

		return result;
	}
};

} // spt::math