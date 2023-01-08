#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None					= 0,
	ShadowGenerationStage	= BIT(0),
	GBufferGenerationStage	= BIT(1)
};


struct SceneRenderContext
{
	// For future use
};

} // spt::rsc