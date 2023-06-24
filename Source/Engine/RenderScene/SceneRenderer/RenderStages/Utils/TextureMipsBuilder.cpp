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

namespace MipsBuilder
{

BEGIN_SHADER_STRUCT(MipsBuildPassParams)
	SHADER_STRUCT_FIELD(Uint32, downsampleMipsNum)
END_SHADER_STRUCT();


DS_BEGIN(MipsBuildPassDS, rg::RGDescriptorSetState<MipsBuildPassDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<MipsBuildPassParams>),				u_mipsBuildParams)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_inputSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>),								u_inputTexture)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),								u_textureMip0)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<math::Vector4f>),						u_textureMip1)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<math::Vector4f>),						u_textureMip2)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<math::Vector4f>),						u_textureMip3)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWTexture2DBinding<math::Vector4f>),						u_textureMip4)
DS_END();


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
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color, mipIdx, 1);
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

		const lib::SharedRef<MipsBuildPassDS> mipsBuildDS = rdr::ResourcesManager::CreateDescriptorSetState<MipsBuildPassDS>(RENDERER_RESOURCE_NAME(std::format("BuildHiZDS (Mips {} - {})", mipIdx0 + sourceMipLevel, mipIdx0 + downsampleMipsNum + sourceMipLevel)));
		mipsBuildDS->u_mipsBuildParams = params;
		mipsBuildDS->u_inputTexture = inputTexture;
		mipsBuildDS->u_textureMip0 = textureMipViews[mipIdx];
		if (downsampleMipsNum >= 2)
		{
			mipsBuildDS->u_textureMip1 = textureMipViews[mipIdx + 1];
		}
		if (downsampleMipsNum >= 3)
		{
			mipsBuildDS->u_textureMip2 = textureMipViews[mipIdx + 2];
		}
		if (downsampleMipsNum >= 4)
		{
			mipsBuildDS->u_textureMip3 = textureMipViews[mipIdx + 3];
		}
		if (downsampleMipsNum >= 5)
		{
			mipsBuildDS->u_textureMip4 = textureMipViews[mipIdx + 4];
		}

		const math::Vector2u outputRes(std::max(sourceMipResolution.x() >> mipIdx, 1u), std::max(sourceMipResolution.y() >> mipIdx, 1u));
		const math::Vector3u groupCount(math::Utils::DivideCeil(outputRes.x(), 16u), math::Utils::DivideCeil(outputRes.y(), 16u), 1u);
		graphBuilder.Dispatch(RG_DEBUG_NAME(std::format("Build HiZ (Mips ({} - {})", mipIdx, mipIdx0 + downsampleMipsNum)),
							  buildMipsPipeline, 
							  groupCount,
							  rg::BindDescriptorSets(mipsBuildDS));

		inputTexture = textureMipViews[mipIdx + downsampleMipsNum - 1];
	}
}

} // MipsBuilder

} // spt::rsc
