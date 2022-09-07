#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class SPIRVOptimizer
{
public:

	static lib::DynamicArray<Uint32> OptimizeBinary(const lib::DynamicArray<Uint32>& binary);
};

} // spt::sc