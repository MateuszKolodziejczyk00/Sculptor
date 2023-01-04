#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None					= 0,
	ShadowGenerationStage	= BIT(0),
	BasePassStage			= BIT(1)
};

} // spt::rsc