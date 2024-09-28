#include "hiZRenderer.h"
#include "View/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "DescriptorSetBindings/RWTextureBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

namespace HiZ
{

BEGIN_SHADER_STRUCT(BuildHiZParams)
	SHADER_STRUCT_FIELD(Uint32, downsampleMipsNum)
END_SHADER_STRUCT();


DS_BEGIN(BuildHiZDS, rg::RGDescriptorSetState<BuildHiZDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<BuildHiZParams>),                            u_buildParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_depthSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_depthTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<Real32>),                                       u_HiZMip0)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),                               u_HiZMip1)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),                               u_HiZMip2)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),                               u_HiZMip3)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<Real32>),                               u_HiZMip4)
DS_END();


static rdr::PipelineStateID CompileBuildHiZPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/DepthPrepass/BuildHiZ.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildHiZCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BuildHiZPipeline"), shader);
}

HiZSizeInfo ComputeHiZSizeInfo(const math::Vector2u& resolution)
{
	HiZSizeInfo result;
	result.resolution = math::Vector2u(math::Utils::PreviousPowerOf2(resolution.x()), math::Utils::PreviousPowerOf2(resolution.y()));
	result.mipLevels = math::Utils::ComputeMipLevelsNumForResolution(result.resolution);
	return result;
}

void CreateHierarchicalZ(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle depthTexture, rg::RGTextureHandle hiZ)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "Hi-Z");

	const math::Vector2u hiZRes = hiZ->GetResolution2D();
	const Uint32 mipLevels      = hiZ->GetTextureDefinition().mipLevels;

	lib::DynamicArray<rg::RGTextureViewHandle> hiZMipViews;
	hiZMipViews.reserve(static_cast<SizeType>(mipLevels));

	for (Uint32 mipIdx = 0; mipIdx < mipLevels; ++mipIdx)
	{
		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, mipIdx, 1);
		hiZMipViews.emplace_back(graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("HiZ Mip({})", mipIdx)), hiZ, viewDef));
	}

	static const rdr::PipelineStateID buildHiZPipeline = CompileBuildHiZPipeline();

	rg::RGTextureViewHandle inputDepthTexture = depthTexture;

	for (SizeType mipIdx = 0; mipIdx < static_cast<SizeType>(mipLevels); mipIdx += 5)
	{
		const Uint32 mipIdx0 = static_cast<Uint32>(mipIdx);
		const Uint32 downsampleMipsNum = std::min<Uint32>(mipLevels - mipIdx0, 5u);

		SPT_CHECK(downsampleMipsNum >= 1 && downsampleMipsNum <= 5);

		BuildHiZParams params;
		params.downsampleMipsNum = downsampleMipsNum;

		const lib::MTHandle<BuildHiZDS> buildHiZDS = graphBuilder.CreateDescriptorSet<BuildHiZDS>(RENDERER_RESOURCE_NAME(std::format("BuildHiZDS (Mips {} - {})", mipIdx, mipIdx0 + downsampleMipsNum)));
		buildHiZDS->u_buildParams = params;
		buildHiZDS->u_depthTexture = inputDepthTexture;
		buildHiZDS->u_HiZMip0 = hiZMipViews[mipIdx];
		if (downsampleMipsNum >= 2)
		{
			buildHiZDS->u_HiZMip1 = hiZMipViews[mipIdx + 1];
		}
		if (downsampleMipsNum >= 3)
		{
			buildHiZDS->u_HiZMip2 = hiZMipViews[mipIdx + 2];
		}
		if (downsampleMipsNum >= 4)
		{
			buildHiZDS->u_HiZMip3 = hiZMipViews[mipIdx + 3];
		}
		if (downsampleMipsNum >= 5)
		{
			buildHiZDS->u_HiZMip4 = hiZMipViews[mipIdx + 4];
		}

		const math::Vector2u outputRes(std::max(hiZRes.x() >> mipIdx, 1u), std::max(hiZRes.y() >> mipIdx, 1u));
		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 16u), math::Utils::DivideCeil(outputRes.y(), 16u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Build HiZ (Mips ({} - {})", mipIdx, mipIdx0 + downsampleMipsNum)),
							  buildHiZPipeline, 
							  groupCount,
							  rg::BindDescriptorSets(buildHiZDS));

		inputDepthTexture = hiZMipViews[mipIdx + downsampleMipsNum - 1];
	}

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
}

} // HiZ

} // spt::rsc
