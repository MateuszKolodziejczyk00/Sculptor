#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructs.h"
#include "SceneRenderer/RenderStages/Utils/TracesAllocator.h"


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
math::Vector2u         ComputeReservoirsResolution(math::Vector2u resolution);
rg::RGBufferViewHandle CreateReservoirsBuffer(rg::RenderGraphBuilder& graphBuilder, math::Vector2u rservoirsResolution);


struct SpatialResamplingPassParams
{
	Real32 resamplingRangeMultiplier        = 1.f; // (0.f, inf)
	Uint32 samplesNum                       = 3u;
	Bool   enableScreenSpaceVisibilityTrace = true;
	Bool   resampleOnlyFromTracedPixels     = true;
};


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
	rg::RGTextureViewHandle baseColorTexture;

	vrt::TracesAllocation   tracesAllocation;
	rg::RGTextureViewHandle vrReprojectionSuccessMask;

	rg::RGTextureViewHandle historyDepthTexture;
	rg::RGTextureViewHandle historyNormalsTexture;
	rg::RGTextureViewHandle historyRoughnessTexture;
	rg::RGTextureViewHandle historyBaseColorTexture;

	rg::RGTextureViewHandle outSpecularLuminanceDistTexture;
	rg::RGTextureViewHandle outDiffuseLuminanceDistTexture;
	rg::RGTextureViewHandle outLightDirectionTexture;
	
	rg::RGTextureViewHandle motionTexture;

	rg::RGTextureViewHandle historySpecularHitDist;

	rg::RGTextureViewHandle preintegratedBRDFLUT;

	lib::Span<const SpatialResamplingPassParams> spatialResamplingPasses;

	Bool enableTemporalResampling = true;

	Bool doFullFinalVisibilityCheck = true;

	Bool enableSecondTracingPass = false;

	Bool enableHitDistanceBasedMaxAge = true;

	Bool enableFireflyFilter = true;

	Uint32 variableRateTileSizeBitOffset = 1u;

	Uint32 reservoirMaxAge = 10u;

	Real32 resamplingRangeStep = 4.f;
};


struct InitialResamplingResult
{
	rg::RGBufferViewHandle resampledReservoirsBuffer;

	vrt::TracesAllocation additionalTracesAllocation;
};


class SpatiotemporalResampler
{
public:

	explicit SpatiotemporalResampler();

	InitialResamplingResult ExecuteInitialResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params);
	void                    ExecuteFinalResampling(rg::RenderGraphBuilder& graphBuilder, const ResamplingParams& params, const InitialResamplingResult& initialResamplingResult);

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