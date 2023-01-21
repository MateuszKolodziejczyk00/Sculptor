#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

struct DepthPrepassResult
{
	rg::RGTextureViewHandle depth;
};

struct ForwardOpaquePassResult
{
	rg::RGTextureViewHandle radiance;
	rg::RGTextureViewHandle normals;
};

} // spt::rsc