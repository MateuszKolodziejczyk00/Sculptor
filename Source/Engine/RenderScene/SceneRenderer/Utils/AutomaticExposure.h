#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RHICore/RHITextureTypes.h"
#include "SceneRenderer/RenderStages/Utils/BilateralGridRenderer.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class ViewRenderingSpec;
class RenderScene;


namespace automatic_exposure
{

struct Inputs
{
	Real32 minLogLuminance;
	Real32 maxLogLuminance;

	rg::RGTextureViewHandle linearColor;

	rg::RGBufferViewHandle viewExposureData;
};


struct Outputs
{
	bilateral_grid::BilateralGridInfo bilateralGridInfo;
};


Outputs RenderAutoExposure(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const Inputs& inputs);

} // automatic_exposure

} // spt::rsc
