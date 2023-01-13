#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct GBufferData
{
	rg::RGTextureViewHandle depth;
	//rg::RGTextureViewHandle tangentFrame;
	//rg::RGTextureViewHandle uvsAndDepthGradients;
	//rg::RGTextureViewHandle uvGradients;

	rg::RGTextureViewHandle testColor;
};

} // spt::rsc