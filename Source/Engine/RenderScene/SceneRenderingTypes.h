#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None					= 0,
	ShadowMap				= BIT(0),
	DepthPrepass			= BIT(1),
	ForwardOpaque			= BIT(2),
	HDRResolve				= BIT(3)
};

} // spt::rsc