#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

namespace EShaderStage
{

enum Flags : Flags32
{
	None			= 0,
	Vertex			= BIT(0),
	Fragment		= BIT(1),
	Compute			= BIT(2)
};

}

}