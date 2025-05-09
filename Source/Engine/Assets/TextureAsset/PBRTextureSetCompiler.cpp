#include "PBRTextureSetCompiler.h"
#include "MathUtils.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcesPool.h"
#include "Renderer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/RWTextureBinding.h"


namespace spt::as
{

namespace copy_to_pbr_textures
{

DS_BEGIN(CopyToPBRTexturesDS, rg::RGDescriptorSetState<CopyToPBRTexturesDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_loadedBaseColor)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_loadedMetallicRoughness)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector4f>), u_loadedNormals)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),  u_rwBaseColor)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector2f>),  u_rwMetallicRoughness)
	DS_BINDING(BINDING_TYPE(gfx::RWTexture2DBinding<math::Vector4f>),  u_rwNormals)
DS_END();


static rdr::PipelineStateID CompileCopyToPBRTexturesPipeline()
{
	const rdr::ShaderID shader = rdr::ResourcesManager::CreateShader("Sculptor/PBRTexturesCompiler/CopyLoadedTextures.hlsl", sc::ShaderStageCompilationDef(rhi::EShaderStage::Compute, "CopyCS"));
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CopyLoadedTextures"), shader);
}


struct CopyToPBRTexturesParams
{
	rg::RGTextureViewHandle loadedBaseColorTexture;
	rg::RGTextureViewHandle loadedMetallicRoughnessTexture;
	rg::RGTextureViewHandle loadedNormalsTexture;

	rg::RGTextureViewHandle outBaseColorTexture;
	rg::RGTextureViewHandle outMetallicRoughnessTexture;
	rg::RGTextureViewHandle outNormalsTexture;
};


static void Copy(rg::RenderGraphBuilder& graphBuilder, const CopyToPBRTexturesParams& params)
{
	SPT_PROFILER_FUNCTION();

	lib::MTHandle<CopyToPBRTexturesDS> copyToPBRTexturesDS = graphBuilder.CreateDescriptorSet<CopyToPBRTexturesDS>(RENDERER_RESOURCE_NAME("CopyToPBRTexturesDS"));
	copyToPBRTexturesDS->u_loadedBaseColor         = params.loadedBaseColorTexture;
	copyToPBRTexturesDS->u_loadedMetallicRoughness = params.loadedMetallicRoughnessTexture;
	copyToPBRTexturesDS->u_loadedNormals           = params.loadedNormalsTexture;
	copyToPBRTexturesDS->u_rwBaseColor             = params.outBaseColorTexture;
	copyToPBRTexturesDS->u_rwMetallicRoughness     = params.outMetallicRoughnessTexture;
	copyToPBRTexturesDS->u_rwNormals               = params.outNormalsTexture;

	static const rdr::PipelineStateID pipelineState = CompileCopyToPBRTexturesPipeline();

	const math::Vector2u resolution = params.loadedBaseColorTexture->GetResolution2D();

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(resolution.x(), 8u), math::Utils::DivideCeil(resolution.y(), 8u), 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Copy To PBR Textures"),
						  pipelineState,
						  dispatchGroupsNum,
						  rg::BindDescriptorSets(std::move(copyToPBRTexturesDS)));
}

} // copy_to_pbr_textures

PBRCompiledTextureSetData PBRTextureSetCompiler::CompileTextureSet(const PBRTextureSetCompilationParams& params, const DDC& ddc)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector2u texturesResolution = params.baseColor->GetResolution2D();

	// For now assume that all textures have the same resolution
	SPT_CHECK(texturesResolution == params.metallicRoughness->GetResolution2D());
	SPT_CHECK(texturesResolution == params.normals->GetResolution2D());

	const Uint32 mipLevelsNum = math::Utils::ComputeMipLevelsNumForResolution(texturesResolution);

	rg::RenderGraphResourcesPool renderResourcesPool;
	rg::RenderGraphBuilder graphBuilder(renderResourcesPool);

	const rg::RGTextureViewHandle loadedBaseColor         = graphBuilder.AcquireExternalTextureView(params.baseColor);
	const rg::RGTextureViewHandle loadedMetallicRoughness = graphBuilder.AcquireExternalTextureView(params.metallicRoughness);
	const rg::RGTextureViewHandle loadedNormals           = graphBuilder.AcquireExternalTextureView(params.normals);

	const rg::RGTextureViewHandle baseColor         = graphBuilder.CreateTextureView(RG_DEBUG_NAME("BaseColor"), rg::TextureDef(texturesResolution, rhi::EFragmentFormat::RGBA8_UN_Float).SetMipLevelsNum(mipLevelsNum));
	const rg::RGTextureViewHandle metallicRoughness = graphBuilder.CreateTextureView(RG_DEBUG_NAME("MetallicRoughness"), rg::TextureDef(texturesResolution, rhi::EFragmentFormat::RG8_UN_Float).SetMipLevelsNum(mipLevelsNum));
	const rg::RGTextureViewHandle normals           = graphBuilder.CreateTextureView(RG_DEBUG_NAME("Normals"), rg::TextureDef(texturesResolution, rhi::EFragmentFormat::RGBA8_UN_Float).SetMipLevelsNum(mipLevelsNum));

	copy_to_pbr_textures::Copy(graphBuilder,
							   {
								   .loadedBaseColorTexture         = loadedBaseColor,
								   .loadedMetallicRoughnessTexture = loadedMetallicRoughness,
								   .loadedNormalsTexture           = loadedNormals,
								   .outBaseColorTexture            = baseColor,
								   .outMetallicRoughnessTexture    = metallicRoughness,
								   .outNormalsTexture              = normals
							   });

	for (Uint32 mipIdx = 1u; mipIdx < mipLevelsNum; ++mipIdx)
	{
		graphBuilder.BlitTexture(RG_DEBUG_NAME("Generate Base Color Mip"),
								 graphBuilder.CreateTextureMipView(baseColor, mipIdx - 1u),
								 graphBuilder.CreateTextureMipView(baseColor, mipIdx),
								 rhi::ESamplerFilterType::Linear);

		graphBuilder.BlitTexture(RG_DEBUG_NAME("Generate Metallic Roughness Mip"),
								 graphBuilder.CreateTextureMipView(metallicRoughness, mipIdx - 1u),
								 graphBuilder.CreateTextureMipView(metallicRoughness, mipIdx),
								 rhi::ESamplerFilterType::Linear);

		graphBuilder.BlitTexture(RG_DEBUG_NAME("Generate Normals Mip"),
								 graphBuilder.CreateTextureMipView(normals, mipIdx - 1u),
								 graphBuilder.CreateTextureMipView(normals, mipIdx),
								 rhi::ESamplerFilterType::Linear);
	}

	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> baseColorMipsData;
	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> metallicRoughnessMipsData;
	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> normalsMipsData;
	baseColorMipsData.reserve(mipLevelsNum);
	metallicRoughnessMipsData.reserve(mipLevelsNum);
	normalsMipsData.reserve(mipLevelsNum);

	for (Uint32 mipIdx = 0u; mipIdx < mipLevelsNum; ++mipIdx)
	{
		baseColorMipsData.emplace_back(graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Base Color Mip Data"), graphBuilder.CreateTextureMipView(baseColor, mipIdx)));
		metallicRoughnessMipsData.emplace_back(graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Metallic Roughness Mip Data"), graphBuilder.CreateTextureMipView(metallicRoughness, mipIdx)));
		normalsMipsData.emplace_back(graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Normals Mip Data"), graphBuilder.CreateTextureMipView(normals, mipIdx)));
	}

	const auto cacheTextureDefinition = [](const rg::RGTextureViewHandle& textureView)
	{
		return TextureDefinition
		{
			.resolution   = textureView->GetResolution(),
			.format       = textureView->GetFormat(),
			.mipLevelsNum = textureView->GetMipLevelsNum()
		};
	};

	PBRCompiledTextureSetData compiledData;
	compiledData.baseColor.definition         = cacheTextureDefinition(baseColor);
	compiledData.metallicRoughness.definition = cacheTextureDefinition(metallicRoughness);
	compiledData.normals.definition           = cacheTextureDefinition(normals);

	graphBuilder.Execute();

	rdr::Renderer::WaitIdle();

	Uint32 accumulatedDataSize = 0u;

	const auto accmulateTextureOffset = [&accumulatedDataSize](CompiledTexture& texture, const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>>& mipsData)
	{
		texture.mips.reserve(mipsData.size());

		Uint32 mipsOffset = accumulatedDataSize;
		for (const lib::SharedPtr<rdr::Buffer>& mipData : mipsData)
		{
			const Uint32 mipSize = static_cast<Uint32>(mipData->GetRHI().GetSize());

			texture.mips.emplace_back(CompiledMip{.offset = mipsOffset, .size = mipSize});

			mipsOffset += static_cast<Uint32>(mipSize);
		}

		accumulatedDataSize = mipsOffset;
	};

	accmulateTextureOffset(compiledData.baseColor, baseColorMipsData);
	accmulateTextureOffset(compiledData.metallicRoughness, metallicRoughnessMipsData);
	accmulateTextureOffset(compiledData.normals, normalsMipsData); 
	const DDCResourceHandle ddcResHandle = ddc.CreateDerivedData(accumulatedDataSize);

	lib::Span<Byte> ddcResourceData = ddcResHandle.GetMutableSpan();

	const auto uploadMipsData = [&ddcResourceData](const CompiledTexture& texture, const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>>& mipsData)
	{
		for (Uint32 mipIdx = 0u; mipIdx < mipsData.size(); ++mipIdx)
		{
			const rhi::RHIMappedByteBuffer mipData(mipsData[mipIdx]->GetRHI());

			const Uint32 mipOffset = texture.mips[mipIdx].offset;

			SPT_CHECK(texture.mips[mipIdx].size == mipData.GetSize());

			std::memcpy(ddcResourceData.data() + mipOffset, mipData.GetPtr(), mipData.GetSize());
		}
	};

	compiledData.derivedDataKey = ddcResHandle.GetKey();

	return compiledData;
}

} // spt::as
