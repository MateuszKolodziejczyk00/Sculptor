#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderView;


namespace sr_restir
{

BEGIN_ALIGNED_SHADER_STRUCT(16u, SRPackedReservoir)
	SHADER_STRUCT_FIELD(math::Vector3f, hitLocation)
	SHADER_STRUCT_FIELD(Uint32,         packedLuminance)
	SHADER_STRUCT_FIELD(math::Vector2f, hitNormal) // octahedral encoding
	SHADER_STRUCT_FIELD(Real32,         weight)
	SHADER_STRUCT_FIELD(Uint32,         MAndProps)
END_SHADER_STRUCT();


rg::RGBufferViewHandle CreateReservoirsBuffer(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution);


struct ResamplingParams
{
	explicit ResamplingParams(const RenderView& inRenderView)
		: renderView(inRenderView)
	{
	}

	const RenderView& renderView;

	rg::RGBufferViewHandle reservoirBuffer;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle normalsTexture;
	rg::RGTextureViewHandle roughnessTexture;
	rg::RGTextureViewHandle specularColorTexture;

	rg::RGTextureViewHandle outLuminanceHitDistanceTexture;

	Uint32 spatialResamplingIterations = 1u;
};


class SpatiotemporalResampler
{
public:

	explicit SpatiotemporalResampler();

	void Resample(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params);
};

} // sr_restir

} // spt::rsc