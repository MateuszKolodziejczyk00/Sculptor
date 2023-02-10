#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None					= 0,
	DepthPrepass			= BIT(0),
	ForwardOpaque			= BIT(1),
	HDRResolve				= BIT(2)
};


struct SceneRenderContext
{
	// For future use
};

} // spt::rsc