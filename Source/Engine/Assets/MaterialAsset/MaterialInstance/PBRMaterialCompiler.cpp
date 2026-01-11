#include "PBRMaterialCompiler.h"
#include "PBRMaterialRuntime.h"
#include "MathUtils.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "RHICore/RHITextureTypes.h"
#include "RendererUtils.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcesPool.h"
#include "Renderer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Loaders/TextureLoader.h"
#include "Transfers/UploadsManager.h"
#include "PBRMaterialInstance.h"
#include "Bindless/BindlessTypes.h"
#include "PBRMaterialRuntime.h"

SPT_DEFINE_LOG_CATEGORY(PBRMaterialCompiler, true);


namespace spt::as
{

COMPUTE_PSO(CompilePBRMaterialTexturesPSO)
{
	COMPUTE_SHADER("Sculptor/Materials/CompilePBRMaterialTextures.hlsl", CompilePBRMaterialTexturesCS);

	PRESET(dflt);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		dflt = CompilePSO(compiler, { });
	}
};


BEGIN_SHADER_STRUCT(CompilePBRMaterialConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, loadedBaseColor)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, loadedMetallicRoughness)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, loadedNormals)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, loadedEmissive)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwBaseColor)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, rwMetallicRoughness)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector3f>, rwNormals)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwEmissive)
END_SHADER_STRUCT();


DS_BEGIN(CompilePBRMaterialTexturesDS, rg::RGDescriptorSetState<CompilePBRMaterialTexturesDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CompilePBRMaterialConstants>), u_constants)
DS_END();


static void CompilePBRMaterialTextures(rg::RenderGraphBuilder& graphBuilder, math::Vector2u dispatchSize, const CompilePBRMaterialConstants& params)
{
	SPT_PROFILER_FUNCTION();

	lib::MTHandle<CompilePBRMaterialTexturesDS> ds = graphBuilder.CreateDescriptorSet<CompilePBRMaterialTexturesDS>(RENDERER_RESOURCE_NAME("CompilePBRMaterialTexturesDS"));
	ds->u_constants = params;

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(dispatchSize.x(), 16u), math::Utils::DivideCeil(dispatchSize.y(), 16u), 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compile PBR Material Textures"),
						  CompilePBRMaterialTexturesPSO::dflt,
						  dispatchGroupsNum,
						  rg::BindDescriptorSets(std::move(ds)));
}


static lib::SharedPtr<rdr::TextureView> TryLoadTexture(const lib::Path& path)
{
	const rhi::ETextureUsage loadedTexturesUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::TransferDest);

	lib::SharedPtr<rdr::Texture> loadedTexture;

	if (!path.empty())
	{
		loadedTexture = gfx::TextureLoader::LoadTexture(path.generic_string(), loadedTexturesUsage);

		if (!loadedTexture)
		{
			SPT_LOG_ERROR(PBRMaterialCompiler, "Failed to load texture from path: {}", path.generic_string());
		}
	}

	return loadedTexture ? loadedTexture->CreateView(RENDERER_RESOURCE_NAME(path.generic_string())) : lib::SharedPtr<rdr::TextureView>{};
}

#pragma optimize("", off)
lib::DynamicArray<Byte> CompilePBRMaterial(const AssetInstance& asset, const PBRMaterialDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	PBRMaterialDataHeader headerData;
	headerData.baseColorFactor = definition.baseColorFactor;
	headerData.metallicFactor  = definition.metallicFactor;
	headerData.roughnessFactor = definition.roughnessFactor;
	headerData.emissionFactor  = definition.emissionFactor;
	headerData.doubleSided     = definition.doubleSided   ? 1u : 0u;
	headerData.customOpacity   = definition.customOpacity ? 1u : 0u;

	lib::DynamicArray<Byte> compiledData;
	compiledData.resize(sizeof(PBRMaterialDataHeader));

	const lib::Path referencePath         = asset.GetDirectoryPath();
	const lib::Path baseColorPath         = !definition.baseColorTexPath.empty() ? (referencePath / definition.baseColorTexPath) : lib::Path{};
	const lib::Path metallicRoughnessPath = !definition.metallicRoughnessTexPath.empty() ? (referencePath / definition.metallicRoughnessTexPath) : lib::Path{};
	const lib::Path normalsPath           = !definition.normalsTexPath.empty() ? (referencePath / definition.normalsTexPath) : lib::Path{};
	const lib::Path emissivePath          = !definition.emissiveTexPath.empty() ? (referencePath / definition.emissiveTexPath) : lib::Path{};

	const Bool usesAnyTexture = !baseColorPath.empty() || !metallicRoughnessPath.empty() || !normalsPath.empty() || !emissivePath.empty();

	if (usesAnyTexture)
	{
		const lib::SharedPtr<rdr::TextureView> loadedBaseColorTexture         = TryLoadTexture(baseColorPath);
		const lib::SharedPtr<rdr::TextureView> loadedEmissiveTexture          = TryLoadTexture(emissivePath);
		const lib::SharedPtr<rdr::TextureView> loadedMetallicRoughnessTexture = TryLoadTexture(metallicRoughnessPath);
		const lib::SharedPtr<rdr::TextureView> loadedNormalsTexture           = TryLoadTexture(normalsPath);

		gfx::UploadsManager::Get().FlushPendingUploads();

		rg::RenderGraphResourcesPool renderResourcesPool;
		rg::RenderGraphBuilder graphBuilder(renderResourcesPool);

		math::Vector2u maxDimentions = math::Vector2u::Zero();

		const auto acquireRGTextures = [&](const lib::SharedPtr<rdr::TextureView>& texture, rhi::EFragmentFormat targetFormat) -> std::pair<rg::RGTextureViewHandle, rg::RGTextureViewHandle>
		{
			if (texture)
			{
				const rg::RGTextureViewHandle loadedTex = graphBuilder.AcquireExternalTextureView(texture);

				const math::Vector2u resolution = loadedTex->GetResolution2D();
				const Uint32 mipLevelsNum       = math::Utils::ComputeMipLevelsNumForResolution(resolution);
				const rg::RGTextureViewHandle tex = graphBuilder.CreateTextureView(RG_DEBUG_NAME(texture->GetRHI().GetName()), rg::TextureDef(resolution, targetFormat).SetMipLevelsNum(mipLevelsNum));

				maxDimentions = maxDimentions.cwiseMax(resolution);

				return std::make_pair(loadedTex, tex);
			}

			return {};
		};

		const auto [loadedBaseColor, baseColor]                 = acquireRGTextures(loadedBaseColorTexture, rhi::EFragmentFormat::RGBA8_UN_Float);
		const auto [loadedEmissive, emissive]                   = acquireRGTextures(loadedEmissiveTexture, rhi::EFragmentFormat::RGBA8_UN_Float);
		const auto [loadedMetallicRoughness, metallicRoughness] = acquireRGTextures(loadedMetallicRoughnessTexture, rhi::EFragmentFormat::RG8_UN_Float);
		const auto [loadedNormals, normals]                     = acquireRGTextures(loadedNormalsTexture, rhi::EFragmentFormat::RGBA8_UN_Float);

		CompilePBRMaterialConstants compilationConstants;
		compilationConstants.loadedBaseColor         = loadedBaseColor;
		compilationConstants.loadedMetallicRoughness = loadedMetallicRoughness;
		compilationConstants.loadedNormals           = loadedNormals;
		compilationConstants.loadedEmissive          = loadedEmissive;
		compilationConstants.rwBaseColor             = baseColor;
		compilationConstants.rwMetallicRoughness     = metallicRoughness;
		compilationConstants.rwNormals               = normals;
		compilationConstants.rwEmissive              = emissive;

		CompilePBRMaterialTextures(graphBuilder, maxDimentions, compilationConstants);

		const auto generateTextureMipsData = [&](rg::RGTextureViewHandle texture) -> lib::DynamicArray<lib::SharedPtr<rdr::Buffer>>
		{ 
			lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> mipsData;

			if (texture.IsValid())
			{
				const Uint32 mipLevelsNum = texture->GetMipLevelsNum();

				for (Uint32 mipIdx = 1u; mipIdx < mipLevelsNum; ++mipIdx)
				{
					graphBuilder.BlitTexture(RG_DEBUG_NAME("Generate Mip"),
											 graphBuilder.CreateTextureMipView(texture, mipIdx - 1u),
											 graphBuilder.CreateTextureMipView(texture, mipIdx),
											 rhi::ESamplerFilterType::Linear);
				}

				mipsData.reserve(mipLevelsNum);
				for (Uint32 mipIdx = 0u; mipIdx < mipLevelsNum; ++mipIdx)
				{
					mipsData.emplace_back(graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Mip Data"), graphBuilder.CreateTextureMipView(texture, mipIdx)));
				}
			}

			return mipsData;
		};

		const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> baseColorMipsData         = generateTextureMipsData(baseColor);
		const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> metallicRoughnessMipsData = generateTextureMipsData(metallicRoughness);
		const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> normalsMipsData           = generateTextureMipsData(normals);
		const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> emissiveMipsData          = generateTextureMipsData(emissive);

		graphBuilder.Execute();

		const auto cacheTextureDefinition = [](MaterialTextureDefinition& textureDefinition, const rg::RGTextureViewHandle& textureView)
		{
			if (textureView)
			{
				textureDefinition.resolution   = textureView->GetResolution2D();
				textureDefinition.format       = textureView->GetFormat();
				textureDefinition.mipLevelsNum = textureView->GetMipLevelsNum();
			}
		};

		cacheTextureDefinition(headerData.baseColorTexture, baseColor);
		cacheTextureDefinition(headerData.metallicRoughnessTexture, metallicRoughness);
		cacheTextureDefinition(headerData.normalsTexture, normals);
		cacheTextureDefinition(headerData.emissiveTexture, emissive);

		rdr::Renderer::WaitIdle();

		const Uint32 texturesDataOffset = static_cast<Uint32>(compiledData.size());
		Uint32 accumulatedDataOffset = texturesDataOffset;

		const auto accmulateTextureOffset = [&accumulatedDataOffset](MaterialTextureDefinition& textureDef, const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>>& mipsData)
		{
			SPT_CHECK(mipsData.size() < MaterialTextureDefinition::s_maxMipLevelsNum);

			Uint32 mipsOffset = accumulatedDataOffset;
			for (Uint32 mipIdx = 0u; mipIdx < mipsData.size(); ++mipIdx)
			{
				const lib::SharedPtr<rdr::Buffer>& mipData = mipsData[mipIdx];
				const Uint32 mipSize = static_cast<Uint32>(mipData->GetRHI().GetSize());

				textureDef.mipLevels[mipIdx] = MaterialTextureMipDefinition{.dataOffset = mipsOffset, .dataSize = mipSize};

				mipsOffset += static_cast<Uint32>(mipSize);
			}

			accumulatedDataOffset += mipsOffset;
		};

		accmulateTextureOffset(headerData.baseColorTexture, baseColorMipsData);
		accmulateTextureOffset(headerData.metallicRoughnessTexture,  metallicRoughnessMipsData);
		accmulateTextureOffset(headerData.normalsTexture,  normalsMipsData);
		accmulateTextureOffset(headerData.emissiveTexture,  emissiveMipsData);

		compiledData.resize(accumulatedDataOffset);

		const auto downloadMipsData = [&compiledData](const MaterialTextureDefinition& texture, const lib::DynamicArray<lib::SharedPtr<rdr::Buffer>>& mipsData)
		{
			for (Uint32 mipIdx = 0u; mipIdx < mipsData.size(); ++mipIdx)
			{
				const rhi::RHIMappedByteBuffer mipData(mipsData[mipIdx]->GetRHI());

				const spt::as::MaterialTextureMipDefinition& mip = texture.mipLevels[mipIdx];

				SPT_CHECK(mip.dataSize == mipData.GetSize());

				std::memcpy(compiledData.data() + mip.dataOffset, mipData.GetPtr(), mipData.GetSize());
			}
		};

		downloadMipsData(headerData.baseColorTexture, baseColorMipsData);
		downloadMipsData(headerData.metallicRoughnessTexture,  metallicRoughnessMipsData);
		downloadMipsData(headerData.normalsTexture,  normalsMipsData);
		downloadMipsData(headerData.emissiveTexture,  emissiveMipsData);
	}

	std::memcpy(compiledData.data(), &headerData, sizeof(PBRMaterialDataHeader));

	return compiledData;
}

} // spt::as
