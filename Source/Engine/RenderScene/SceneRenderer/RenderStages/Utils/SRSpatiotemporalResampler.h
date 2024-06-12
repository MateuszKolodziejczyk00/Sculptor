#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc
{

class RenderView;
class RenderScene;


namespace sr_restir
{

BEGIN_ALIGNED_SHADER_STRUCT(16u, SRPackedReservoir)
	SHADER_STRUCT_FIELD(math::Vector3f, hitLocation)
	SHADER_STRUCT_FIELD(Uint32,         packedLuminance)
	SHADER_STRUCT_FIELD(math::Vector2f, hitNormal) // octahedral encoding
	SHADER_STRUCT_FIELD(Real32,         weight)
	SHADER_STRUCT_FIELD(Uint32,         MAndProps)
END_SHADER_STRUCT();


Uint64                 ComputeReservoirsBufferSize(math::Vector2u resolution);
rg::RGBufferViewHandle CreateReservoirsBuffer(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution);


struct ResamplingParams
{
	explicit ResamplingParams(const RenderView& inRenderView, const RenderScene& inRenderScene);

	math::Vector2u GetResolution() const;

	const RenderView&  renderView;
	const RenderScene& renderScene;

	rg::RGBufferViewHandle initialReservoirBuffer;

	rg::RGTextureViewHandle depthTexture;
	rg::RGTextureViewHandle normalsTexture;
	rg::RGTextureViewHandle roughnessTexture;
	rg::RGTextureViewHandle specularColorTexture;

	rg::RGTextureViewHandle historyDepthTexture;
	rg::RGTextureViewHandle historyNormalsTexture;
	rg::RGTextureViewHandle historyRoughnessTexture;
	rg::RGTextureViewHandle historySpecularColorTexture;

	rg::RGTextureViewHandle outLuminanceHitDistanceTexture;
	
	rg::RGTextureViewHandle motionTexture;

	rg::RGTextureViewHandle preintegratedBRDFLUT;

	Uint32 spatialResamplingIterations = 1u;
	Bool   enableTemporalResampling    = true;
	Bool   enableFinalVisibilityCheck  = true;
};


class SpatiotemporalResampler
{
public:

	explicit SpatiotemporalResampler();

	void Resample(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params);

private:

	Bool HasValidTemporalData(const ResamplingParams& params) const;

	void PrepareForResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params);

	lib::SharedPtr<rdr::Buffer> CreateTemporalReservoirBuffer(rg::RenderGraphBuilder& graphBuilder, const math::Vector2u& resolution) const;

	math::Vector2u m_historyResolution;

	lib::SharedPtr<rdr::Buffer> m_inputTemporalReservoirBuffer;
	lib::SharedPtr<rdr::Buffer> m_outputTemporalReservoirBuffer;
};

} // sr_restir

} // spt::rsc