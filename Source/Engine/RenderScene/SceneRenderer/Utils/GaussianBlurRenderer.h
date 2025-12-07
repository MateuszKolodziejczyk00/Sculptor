#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RHICore/RHITextureTypes.h"
#include "RenderGraphTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

namespace gaussian_blur_renderer
{

struct BlurPassParams
{
	Uint32 kernelSize = 14u;
	Real32 sigma = 4.f;
};


struct GaussianBlur2DParams
{
	GaussianBlur2DParams() = default;

	GaussianBlur2DParams(Real32 sigma, Uint32 xKernel, Uint32 yKernel)
		: horizontalPass({ xKernel, sigma })
		, verticalPass({ yKernel, sigma })
	{
	}

	BlurPassParams horizontalPass;
	BlurPassParams verticalPass;

	Bool useTonemappedValues = false;
};


struct GaussianBlur3DParams : public GaussianBlur2DParams
{
	GaussianBlur3DParams() = default;

	GaussianBlur3DParams(Real32 sigmaXY, Real32 sigmaDepth, Uint32 xKernel, Uint32 yKernel, Uint32 zKernel)
		: GaussianBlur2DParams(sigmaXY, xKernel, yKernel)
		, depthPass({ zKernel, sigmaDepth })
	{
	}

	BlurPassParams depthPass;
};


void ApplyGaussianBlurPass(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, rg::RGTextureViewHandle output, Uint32 dimention, const BlurPassParams& params);

rg::RGTextureViewHandle ApplyGaussianBlur2D(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, const GaussianBlur2DParams& params);
rg::RGTextureViewHandle ApplyGaussianBlur3D(rg::RenderGraphBuilder& graphBuilder, rg::RenderGraphDebugName debugName, rg::RGTextureViewHandle input, const GaussianBlur3DParams& params);

} // gaussian_blur_renderer

} // spt::rsc
