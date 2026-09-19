#include "hiZRenderer.h"
#include "Utils/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

namespace HiZ
{

BEGIN_SHADER_STRUCT(BuildHiZParams)
	SHADER_STRUCT_FIELD(Uint32,                       downsampleMipsNum)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<Real32>, depthTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<Real32>, HiZMip0)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,    HiZMip1)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,    HiZMip2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,    HiZMip3)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,    HiZMip4)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileBuildHiZPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/RenderStages/DepthPrepass/BuildHiZ.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildHiZCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BuildHiZPipeline"), shader);
}

HiZSizeInfo ComputeHiZSizeInfo(const math::Vector2u& resolution)
{
	HiZSizeInfo result;
	result.resolution = math::Vector2u(math::Utils::PreviousPowerOf2(resolution.x()), math::Utils::PreviousPowerOf2(resolution.y()));
	result.mipLevels = rhi::texture_utils::ComputeMipLevelsNumForResolution(result.resolution);
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
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Auto, mipIdx, 1);
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
		params.depthTexture = inputDepthTexture;
		params.HiZMip0 = hiZMipViews[mipIdx];
		if (downsampleMipsNum >= 2)
		{
			params.HiZMip1 = hiZMipViews[mipIdx + 1];
		}
		if (downsampleMipsNum >= 3)
		{
			params.HiZMip2 = hiZMipViews[mipIdx + 2];
		}
		if (downsampleMipsNum >= 4)
		{
			params.HiZMip3 = hiZMipViews[mipIdx + 3];
		}
		if (downsampleMipsNum >= 5)
		{
			params.HiZMip4 = hiZMipViews[mipIdx + 4];
		}

		const math::Vector2u outputRes(std::max(hiZRes.x() >> mipIdx, 1u), std::max(hiZRes.y() >> mipIdx, 1u));
		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 16u), math::Utils::DivideCeil(outputRes.y(), 16u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Build HiZ (Mips ({} - {})", mipIdx, mipIdx0 + downsampleMipsNum)),
							  buildHiZPipeline, 
							  groupCount,
							  rg::ShaderParams(params));

		inputDepthTexture = hiZMipViews[mipIdx + downsampleMipsNum - 1];
	}

	rhi::TextureViewDefinition viewDefinition;
	viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
}

} // HiZ

} // spt::rsc
