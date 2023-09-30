#include "VisibilityMomentsPass.h"
#include "RenderGraphBuilder.h"
#include "MathUtils.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "ResourcesManager.h"


namespace spt::rsc::visibility_denoiser::moments
{

namespace compression
{

DS_BEGIN(VisibilityDataCompressionDS, rg::RGDescriptorSetState<VisibilityDataCompressionDS>)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Uint32>),										u_compressedDataTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
DS_END();


static rdr::PipelineStateID CreateCompressionPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilityDataCompression.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "VisibilityDataCompressionCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Data Compression Pipeline"), shader);
}

rg::RGTextureViewHandle CompressTexture(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u inputResolution = params.dataTexture->GetResolution2D();
	const math::Vector2u compressedResolution = math::Utils::DivideCeil(inputResolution, math::Vector2u(8u, 4u));

	const rg::RGTextureViewHandle compressedTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Compressed", params.debugName.AsString()),
																					 rg::TextureDef(compressedResolution, rhi::EFragmentFormat::R32_U_Int));
																					 

	static const rdr::PipelineStateID pipeline = CreateCompressionPipeline();

	lib::SharedPtr<VisibilityDataCompressionDS> descriptorSet = rdr::ResourcesManager::CreateDescriptorSetState<VisibilityDataCompressionDS>(RENDERER_RESOURCE_NAME("VisibilityDataCompressionDS"));
	descriptorSet->u_compressedDataTexture	= compressedTexture;
	descriptorSet->u_inputTexture			= params.dataTexture;
	
	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: Compress Data", params.debugName.AsString()),
						  pipeline,
						  compressedResolution,
						  rg::BindDescriptorSets(std::move(descriptorSet)));

	return compressedTexture;

}

} // compression

namespace computation
{

DS_BEGIN(VisibilityMomentsComputationDS, rg::RGDescriptorSetState<VisibilityMomentsComputationDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Uint32>),										u_compressedDataTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),										u_momentsTexture)
DS_END();


static rdr::PipelineStateID CreateComputationPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Denoisers/Visibility/VisibilityDataMoments.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "VisibilityDataMomentsCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("Visibility Data Moments Computation Pipeline"), shader);
}

rg::RGTextureViewHandle ComputeMoments(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params, rg::RGTextureViewHandle compressedData)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u resolution = params.dataTexture->GetResolution2D();

	const rg::RGTextureViewHandle momentsTexture = graphBuilder.CreateTextureView(RG_DEBUG_NAME_FORMATTED("{}: Moments", params.debugName.AsString()),
																				  rg::TextureDef(resolution, rhi::EFragmentFormat::R16_UN_Float));

	static const rdr::PipelineStateID pipeline = CreateComputationPipeline();

	lib::SharedPtr<VisibilityMomentsComputationDS> descriptorSet = rdr::ResourcesManager::CreateDescriptorSetState<VisibilityMomentsComputationDS>(RENDERER_RESOURCE_NAME("VisibilityMomentsComputationDS"));
	descriptorSet->u_compressedDataTexture	= compressedData;
	descriptorSet->u_momentsTexture			= momentsTexture;

	graphBuilder.Dispatch(RG_DEBUG_NAME_FORMATTED("{}: Compute Moments", params.debugName.AsString()),
						  pipeline,
						  math::Utils::DivideCeil(resolution, math::Vector2u(8u, 8u)),
						  rg::BindDescriptorSets(std::move(descriptorSet)));

	return momentsTexture;
}

} // computation

rg::RGTextureViewHandle ComputeMoments(rg::RenderGraphBuilder& graphBuilder, const VisibilityMomentsParameters& params)
{
	SPT_PROFILER_FUNCTION();

	const rg::RGTextureViewHandle compressedTexture = compression::CompressTexture(graphBuilder, params);

	return computation::ComputeMoments(graphBuilder, params, compressedTexture);
}

} // spt::rsc::visibility_denoiser::moments
