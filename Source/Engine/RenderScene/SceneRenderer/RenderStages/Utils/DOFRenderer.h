#pragma once

#include "RGResources/RGResourceHandles.h"
#include "Utility/RefCounted.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderScene;
class ViewRenderingSpec;
class RenderViewDS;


namespace dof
{

struct GatherBasedDOFParameters
{
	GatherBasedDOFParameters()
		: focalPlane(3.f)
		, fullFocusRange(1.f)
		, farFocusIncreaseRange(4.f)
		, nearFocusIncreaseRange(1.f)
	{ }

	rg::RGTextureViewHandle linearColorTexture;
	rg::RGTextureViewHandle depthTexture;

	Real32 focalPlane;
	Real32 fullFocusRange;
	Real32 farFocusIncreaseRange;
	Real32 nearFocusIncreaseRange;
};


void RenderGatherBasedDOF(rg::RenderGraphBuilder& graphBuilder, const GatherBasedDOFParameters& params);

} // dof

} // spt::rsc
