#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{

class Pipeline;
class DescriptorSetState;


struct DynamicDescriptorSetInfo
{
	lib::SharedPtr<Pipeline>			pipeline;
	Uint32								dsIdx;
	SizeType							dsHash;
	lib::SharedPtr<DescriptorSetState>	state;
};

} // spt::rdr