#include "hiZRenderer.h"
#include "Utils/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"
#include "ShaderStructs/ShaderStructs.h"
#include "ResourcesManager.h"
#include "Common/ShaderCompilationInput.h"

namespace spt::rsc
{

namespace MipsBuilder
{

BEGIN_SHADER_STRUCT(MipsBuildPassParams)
	SHADER_STRUCT_FIELD(Uint32,                               downsampleMipsNum)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2DRef<math::Vector4f>, inputTexture)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2DRef<math::Vector4f>, textureMip0)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,    textureMip1)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,    textureMip2)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,    textureMip3)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>,    textureMip4)
END_SHADER_STRUCT();


static rdr::PipelineStateID CompileBuildMipsPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/Utils/BuildMips.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "BuildMipsCS"));

	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("BuildMipsPipeline"), shader);
}

void BuildTextureMips(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureHandle texture, Uint32 sourceMipLevel, Uint32 mipLevelsNum)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(sourceMipLevel + mipLevelsNum <= texture->GetTextureDefinition().mipLevels);

	const math::Vector2u textureResolution = texture->GetResolution2D();
	const math::Vector2u sourceMipResolution = math::Vector2u(textureResolution.x() >> sourceMipLevel, textureResolution.y() >> sourceMipLevel);

	lib::DynamicArray<rg::RGTextureViewHandle> textureMipViews;
	textureMipViews.reserve(static_cast<SizeType>(mipLevelsNum));

	for (Uint32 mipIdx = sourceMipLevel; mipIdx < sourceMipLevel + mipLevelsNum; ++mipIdx)
	{
		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Auto, mipIdx, 1);
		textureMipViews.emplace_back(graphBuilder.CreateTextureView(RG_DEBUG_NAME(std::format("HiZ Mip({})", mipIdx)), texture, viewDef));
	}

	static const rdr::PipelineStateID buildMipsPipeline = CompileBuildMipsPipeline();

	rg::RGTextureViewHandle inputTexture = textureMipViews[0];

	for (SizeType mipIdx = 1; mipIdx < static_cast<SizeType>(mipLevelsNum); mipIdx += 5)
	{
		const Uint32 mipIdx0 = static_cast<Uint32>(mipIdx);
		const Uint32 downsampleMipsNum = std::min<Uint32>(mipLevelsNum - mipIdx0, 5u);

		SPT_CHECK(downsampleMipsNum >= 1 && downsampleMipsNum <= 5);

		MipsBuildPassParams params;
		params.downsampleMipsNum = downsampleMipsNum;
		params.inputTexture = inputTexture;
		params.textureMip0 = textureMipViews[mipIdx];
		if (downsampleMipsNum >= 2)
		{
			params.textureMip1 = textureMipViews[mipIdx + 1];
		}
		if (downsampleMipsNum >= 3)
		{
			params.textureMip2 = textureMipViews[mipIdx + 2];
		}
		if (downsampleMipsNum >= 4)
		{
			params.textureMip3 = textureMipViews[mipIdx + 3];
		}
		if (downsampleMipsNum >= 5)
		{
			params.textureMip4 = textureMipViews[mipIdx + 4];
		}

		const math::Vector2u outputRes(std::max(sourceMipResolution.x() >> mipIdx, 1u), std::max(sourceMipResolution.y() >> mipIdx, 1u));
		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 16u), math::Utils::DivideCeil(outputRes.y(), 16u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Build HiZ (Mips ({} - {})", mipIdx, mipIdx0 + downsampleMipsNum)),
							  buildMipsPipeline, 
							  groupCount,
							  rg::ShaderParams(params));

		inputTexture = textureMipViews[mipIdx + downsampleMipsNum - 1];
	}
}

} // MipsBuilder

} // spt::rsc
