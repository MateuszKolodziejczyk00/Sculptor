#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

class GBufferData
{
	rg::RGTextureHandle m_depth;
	rg::RGTextureHandle m_tangentFrame;
	rg::RGTextureHandle m_uvsAndDepthGradients;
	rg::RGTextureHandle m_uvGradients;
};

} // spt::rsc