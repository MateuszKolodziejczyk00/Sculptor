#include "PBRMaterialCompiler.h"
#include "CompiledMaterial.h"
#include "MathUtils.h"
#include "Pipelines/PSOsLibraryTypes.h"
#include "RenderGraphResourcesPool.h"
#include "Transfers/TransfersManager.h"
#include "Transfers/UploadUtils.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "RenderGraphBuilder.h"
#include "Renderer.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "Loaders/TextureLoader.h"
#include "Transfers/UploadsManager.h"
#include "PBRMaterialInstance.h"
#include "Bindless/BindlessTypes.h"
#include "Loaders/GLTF.h"
#include "AssetsSystem.h"
#include "Compression/TextureCompressor.h"


SPT_DEFINE_LOG_CATEGORY(PBRMaterialCompiler, true);

namespace spt::as::material_compiler
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
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         rwAlpha)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwBaseColor)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwMetallicRoughness)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, rwNormals)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwEmissive)
END_SHADER_STRUCT();


static void CompilePBRMaterialTextures(rg::RenderGraphBuilder& graphBuilder, math::Vector2u dispatchSize, const CompilePBRMaterialConstants& params)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(dispatchSize.x(), 16u), math::Utils::DivideCeil(dispatchSize.y(), 16u), 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Compile PBR Material Textures"),
						  CompilePBRMaterialTexturesPSO::dflt,
						  dispatchGroupsNum,
						  rg::EmptyDescriptorSets(),
						  params);
}


COMPUTE_PSO(GeneratePBRTexturesMipsPSO)
{
	COMPUTE_SHADER("Sculptor/Materials/GeneratePBRTexturesMips.hlsl", GeneratePBRTexturesMipsCS);

	PRESET(dflt);

	static void PrecachePSOs(rdr::PSOCompilerInterface& compiler, const rdr::PSOPrecacheParams& params)
	{
		dflt = CompilePSO(compiler, { });
	}
};


BEGIN_SHADER_STRUCT(GeneratePBRTexturesMipsConstants)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         inAlpha)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, inBaseColor)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, inMetallicRoughness)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, inNormals)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, inEmissive)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<Real32>,         rwAlpha)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwBaseColor)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwMetallicRoughness)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector2f>, rwNormals)
	SHADER_STRUCT_FIELD(gfx::UAVTexture2D<math::Vector4f>, rwEmissive)
END_SHADER_STRUCT();


static void GeneratePBRTexturesMips(rg::RenderGraphBuilder& graphBuilder, math::Vector2u dispatchSize, const GeneratePBRTexturesMipsConstants& constants)
{
	SPT_PROFILER_FUNCTION();

	const math::Vector3u dispatchGroupsNum(math::Utils::DivideCeil(dispatchSize.x(), 16u), math::Utils::DivideCeil(dispatchSize.y(), 16u), 1u);

	graphBuilder.Dispatch(RG_DEBUG_NAME("Generate PBR Textures Misp"),
						  GeneratePBRTexturesMipsPSO::dflt,
						  dispatchGroupsNum,
						  rg::EmptyDescriptorSets(),
						  constants);
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


template<typename DownloadMipFunc>
static lib::Span<lib::Span<Byte>> GenericDownloadMipsImpl(const DownloadMipFunc& downloadFunc, lib::MemoryArena& arena, lib::Span<lib::SharedPtr<rdr::Buffer>> mipsData, rg::RGTextureViewHandle textureView)
{
	SPT_PROFILER_FUNCTION();

	if (!textureView.IsValid())
	{
		return {};
	}

	SPT_CHECK(mipsData.size() >= 2u);

	const math::Vector2u resolution = textureView->GetResolution2D();

	lib::Span<lib::Span<Byte>> compressedMips = arena.AllocateSpanUninitialized<lib::Span<Byte>>(mipsData.size() - 2u);

	for (Uint32 mipIdx = 0u; mipIdx < compressedMips.size(); ++mipIdx)
	{
		const math::Vector2u mipResolution = math::Utils::ComputeMipResolution(resolution, mipIdx);

		const rhi::RHIMappedByteBuffer mipData(mipsData[mipIdx]->GetRHI());

		compressedMips[mipIdx] = downloadFunc(arena, mipData.GetSpan(), mipResolution);
	}

	return compressedMips;
}


static lib::Span<lib::Span<Byte>> DownloadMipsAsBC1(lib::MemoryArena& arena, lib::Span<lib::SharedPtr<rdr::Buffer>> mipsData, rg::RGTextureViewHandle textureView)
{
	SPT_PROFILER_FUNCTION();

	const auto downloadBC1 = [](lib::MemoryArena& arena, lib::Span<const Byte> mipData, math::Vector2u mipResolution) -> lib::Span<Byte>
	{
		SPT_CHECK(mipData.size() == mipResolution.x() * mipResolution.y() * 4u); // RGBA8

		const Uint64 sizeBC1 = gfx::compressor::ComputeCompressedSizeBC1(mipResolution);
		lib::Span<Byte> mipDataBC1 = arena.AllocateSpanUninitialized<Byte>(sizeBC1);

		gfx::compressor::CompressSurfaceToBC1(mipDataBC1, gfx::compressor::Surface2D{ mipResolution, mipData });

		return mipDataBC1;
	};

	return GenericDownloadMipsImpl(downloadBC1, arena, mipsData, textureView);
}


static lib::Span<lib::Span<Byte>> DownloadMipsAsBC4(lib::MemoryArena& arena, lib::Span<lib::SharedPtr<rdr::Buffer>> mipsData, rg::RGTextureViewHandle textureView)
{
	SPT_PROFILER_FUNCTION();

	const auto downloadBC4 = [](lib::MemoryArena& arena, lib::Span<const Byte> mipData, math::Vector2u mipResolution) -> lib::Span<Byte>
	{
		SPT_CHECK(mipData.size() == mipResolution.x() * mipResolution.y()); // R8

		const Uint64 sizeBC4 = gfx::compressor::ComputeCompressedSizeBC4(mipResolution);
		lib::Span<Byte> mipDataBC4 = arena.AllocateSpanUninitialized<Byte>(sizeBC4);

		gfx::compressor::CompressSurfaceToBC4(mipDataBC4, gfx::compressor::Surface2D{ mipResolution, mipData });

		return mipDataBC4;
	};

	return GenericDownloadMipsImpl(downloadBC4, arena, mipsData, textureView);
}


static lib::Span<lib::Span<Byte>> DownloadMipsAsBC5(lib::MemoryArena& arena, lib::Span<lib::SharedPtr<rdr::Buffer>> mipsData, rg::RGTextureViewHandle textureView)
{
	SPT_PROFILER_FUNCTION();

	const auto downloadBC5 = [](lib::MemoryArena& arena, lib::Span<const Byte> mipData, math::Vector2u mipResolution) -> lib::Span<Byte>
	{
		SPT_CHECK(mipData.size() == mipResolution.x() * mipResolution.y() * 2u); // RG8

		const Uint64 sizeBC5 = gfx::compressor::ComputeCompressedSizeBC5(mipResolution);
		lib::Span<Byte> mipDataBC5 = arena.AllocateSpanUninitialized<Byte>(sizeBC5);

		gfx::compressor::CompressSurfaceToBC5(mipDataBC5, gfx::compressor::Surface2D{ mipResolution, mipData });

		return mipDataBC5;
	};

	return GenericDownloadMipsImpl(downloadBC5, arena, mipsData, textureView);
}


SPT_MAYBE_UNUSED
static lib::Span<lib::Span<Byte>> DownloadMipsUncompressed(lib::MemoryArena& arena, lib::Span<lib::SharedPtr<rdr::Buffer>> mipsData, rg::RGTextureViewHandle textureView)
{
	SPT_PROFILER_FUNCTION();

	const auto downloadUncompressed = [](lib::MemoryArena& arena, lib::Span<const Byte> mipData, math::Vector2u mipResolution) -> lib::Span<Byte>
	{
		SPT_CHECK(mipData.size() == mipResolution.x() * mipResolution.y() * 4u); // RGBA8

		lib::Span<Byte> mipDataCopy = arena.AllocateSpanUninitialized<Byte>(mipData.size());
		std::memcpy(mipDataCopy.data(), mipData.data(), mipData.size());

		return mipDataCopy;
	};

	return GenericDownloadMipsImpl(downloadUncompressed, arena, mipsData, textureView);
}


struct PBRMaterialCompilationInput
{
	lib::SharedPtr<rdr::TextureView> baseColorTex;
	lib::SharedPtr<rdr::TextureView> metallicRoughnessTex;
	lib::SharedPtr<rdr::TextureView> normalsTex;
	lib::SharedPtr<rdr::TextureView> emissiveTex;

	math::Vector3f baseColorFactor = math::Vector3f::Ones();
	Real32 metallicFactor          = 0.f;
	Real32 roughnessFactor         = 1.f;
	math::Vector3f emissionFactor  = math::Vector3f::Zero();

	Bool doubleSided   = true;
	Bool customOpacity = false;
};


static lib::DynamicArray<Byte> CompilePBRMaterialImpl(const AssetInstance& asset, const PBRMaterialCompilationInput& compilationInput)
{
	SPT_PROFILER_FUNCTION();

	lib::MemoryArena tempArena("PBR Material Compilation Arena", 1024u * 1024u, 512u * 1024u * 1024u);

	PBRMaterialDataHeader headerData;
	headerData.baseColorFactor = compilationInput.baseColorFactor;
	headerData.metallicFactor  = compilationInput.metallicFactor;
	headerData.roughnessFactor = compilationInput.roughnessFactor;
	headerData.emissionFactor  = compilationInput.emissionFactor;
	headerData.doubleSided     = compilationInput.doubleSided   ? 1u : 0u;
	headerData.customOpacity   = compilationInput.customOpacity ? 1u : 0u;

	lib::DynamicArray<Byte> compiledData;
	compiledData.resize(sizeof(PBRMaterialDataHeader));

	const Bool usesAnyTexture = compilationInput.baseColorTex || compilationInput.metallicRoughnessTex || compilationInput.normalsTex || compilationInput.emissiveTex;

	if (usesAnyTexture)
	{
		rg::RenderGraphResourcesPool renderResourcesPool;
		rg::RenderGraphBuilder graphBuilder(tempArena, renderResourcesPool);

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

		const auto [loadedBaseColor, baseColor]                 = acquireRGTextures(compilationInput.baseColorTex,         rhi::EFragmentFormat::RGBA8_UN_Float);
		const auto [loadedEmissive, emissive]                   = acquireRGTextures(compilationInput.emissiveTex,          rhi::EFragmentFormat::RGBA8_UN_Float);
		const auto [loadedMetallicRoughness, metallicRoughness] = acquireRGTextures(compilationInput.metallicRoughnessTex, rhi::EFragmentFormat::RGBA8_UN_Float);
		const auto [loadedNormals, normals]                     = acquireRGTextures(compilationInput.normalsTex,           rhi::EFragmentFormat::RG8_UN_Float);

		rg::RGTextureViewHandle alpha;
		if (baseColor.IsValid() && compilationInput.customOpacity)
		{
			alpha = graphBuilder.CreateTextureView(RG_DEBUG_NAME("PBR Material Alpha"), rg::TextureDef(baseColor->GetResolution2D(), rhi::EFragmentFormat::R8_UN_Float).SetMipLevelsNum(baseColor->GetMipLevelsNum()));
		}

		CompilePBRMaterialConstants compilationConstants;
		compilationConstants.loadedBaseColor         = loadedBaseColor;
		compilationConstants.loadedMetallicRoughness = loadedMetallicRoughness;
		compilationConstants.loadedNormals           = loadedNormals;
		compilationConstants.loadedEmissive          = loadedEmissive;
		compilationConstants.rwAlpha                 = alpha;
		compilationConstants.rwBaseColor             = baseColor;
		compilationConstants.rwMetallicRoughness     = metallicRoughness;
		compilationConstants.rwNormals               = normals;
		compilationConstants.rwEmissive              = emissive;

		CompilePBRMaterialTextures(graphBuilder, maxDimentions, compilationConstants);

		const Uint32 maxMipLevelsNum = math::Utils::ComputeMipLevelsNumForResolution(maxDimentions);

		const auto createTextureMipViews = [&](rg::RGTextureViewHandle texture) -> lib::ManagedSpan<rg::RGTextureViewHandle>
		{
			lib::ManagedSpan<rg::RGTextureViewHandle> mipViews = tempArena.AllocateArray<rg::RGTextureViewHandle>(maxMipLevelsNum);

			if (texture.IsValid())
			{
				const Uint32 mipLevelsNum = texture->GetMipLevelsNum();
				const Uint32 idxOffset = maxMipLevelsNum - mipLevelsNum ;

				for (Uint32 mipIdx = 0u; mipIdx < mipLevelsNum; ++mipIdx)
				{
					mipViews[idxOffset + mipIdx] = graphBuilder.CreateTextureMipView(texture, mipIdx);
				}
			}

			return mipViews;
		};

		const lib::ManagedSpan<rg::RGTextureViewHandle> baseColorMipViews         = createTextureMipViews(baseColor);
		const lib::ManagedSpan<rg::RGTextureViewHandle> metallicRoughnessMipViews = createTextureMipViews(metallicRoughness);
		const lib::ManagedSpan<rg::RGTextureViewHandle> normalsMipViews           = createTextureMipViews(normals);
		const lib::ManagedSpan<rg::RGTextureViewHandle> emissiveMipViews          = createTextureMipViews(emissive);
		const lib::ManagedSpan<rg::RGTextureViewHandle> alphaMipViews             = createTextureMipViews(alpha);

		for (Uint32 dstMipViewIdx = 1u; dstMipViewIdx < maxMipLevelsNum; ++dstMipViewIdx)
		{
			const Uint32 srcMipViewIdx = dstMipViewIdx - 1u;

			math::Vector2u dispatchSize = math::Vector2u::Zero();

			GeneratePBRTexturesMipsConstants mipGenConstants;

			if (alphaMipViews[srcMipViewIdx])
			{
				mipGenConstants.inAlpha = alphaMipViews[srcMipViewIdx];
				mipGenConstants.rwAlpha = alphaMipViews[dstMipViewIdx];
				dispatchSize = dispatchSize.cwiseMax(alphaMipViews[dstMipViewIdx]->GetResolution2D());
			}

			if (baseColorMipViews[srcMipViewIdx])
			{
				mipGenConstants.inBaseColor = baseColorMipViews[srcMipViewIdx];
				mipGenConstants.rwBaseColor = baseColorMipViews[dstMipViewIdx];
				dispatchSize = dispatchSize.cwiseMax(baseColorMipViews[dstMipViewIdx]->GetResolution2D());
			}

			if (metallicRoughnessMipViews[srcMipViewIdx])
			{
				mipGenConstants.inMetallicRoughness = metallicRoughnessMipViews[srcMipViewIdx];
				mipGenConstants.rwMetallicRoughness = metallicRoughnessMipViews[dstMipViewIdx];
				dispatchSize = dispatchSize.cwiseMax(metallicRoughnessMipViews[dstMipViewIdx]->GetResolution2D());
			}

			if (normalsMipViews[srcMipViewIdx])
			{
				mipGenConstants.inNormals = normalsMipViews[srcMipViewIdx];
				mipGenConstants.rwNormals = normalsMipViews[dstMipViewIdx];
				dispatchSize = dispatchSize.cwiseMax(normalsMipViews[dstMipViewIdx]->GetResolution2D());
			}

			if (emissiveMipViews[srcMipViewIdx])
			{
				mipGenConstants.inEmissive = emissiveMipViews[srcMipViewIdx];
				mipGenConstants.rwEmissive = emissiveMipViews[dstMipViewIdx];
				dispatchSize = dispatchSize.cwiseMax(emissiveMipViews[dstMipViewIdx]->GetResolution2D());
			}

			GeneratePBRTexturesMips(graphBuilder, dispatchSize, mipGenConstants);
		}

		const auto stageMipsData = [&](const lib::Span<rg::RGTextureViewHandle>& mipViews) -> lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>>
		{
			// Count valid mips (texture might have "empty" mips at the beginning if it's smaller than max texture size)
			Uint32 mipsNum = 0u;
			for (Uint32 mipIdx = 0u; mipIdx < maxMipLevelsNum; ++mipIdx)
			{
				if (mipViews[mipIdx])
				{
					++mipsNum;
				}
			}

			lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>> mipsData;

			if (mipsNum > 0u)
			{
				mipsData = tempArena.AllocateArray<lib::SharedPtr<rdr::Buffer>>(mipsNum);

				for (Uint32 mipIdx = 0u; mipIdx < mipsNum; ++mipIdx)
				{
					mipsData[mipIdx] = graphBuilder.DownloadTextureToBuffer(RG_DEBUG_NAME("Download Mip Data"), mipViews[mipIdx]);
				}
			}

			return mipsData;
		};

		const lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>> baseColorMipsStagedData         = stageMipsData(baseColorMipViews);
		const lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>> metallicRoughnessMipsStagedData = stageMipsData(metallicRoughnessMipViews);
		const lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>> normalsMipsStagedData           = stageMipsData(normalsMipViews);
		const lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>> emissiveMipsStagedData          = stageMipsData(emissiveMipViews);
		const lib::ManagedSpan<lib::SharedPtr<rdr::Buffer>> alphaMipsStagedData             = stageMipsData(alphaMipViews);

		graphBuilder.Execute();

		rdr::Renderer::WaitIdle();

		const Uint32 texturesDataOffset = static_cast<Uint32>(compiledData.size());
		Uint32 accumulatedDataOffset = texturesDataOffset;

		const lib::Span<lib::Span<Byte>> baseColorMipsData         = DownloadMipsAsBC1(tempArena, baseColorMipsStagedData, baseColor);
		const lib::Span<lib::Span<Byte>> metallicRoughnessMipsData = DownloadMipsAsBC1(tempArena, metallicRoughnessMipsStagedData, metallicRoughness);
		const lib::Span<lib::Span<Byte>> normalsMipsData           = DownloadMipsAsBC5(tempArena, normalsMipsStagedData, normals);
		const lib::Span<lib::Span<Byte>> emissiveMipsData          = DownloadMipsAsBC1(tempArena, emissiveMipsStagedData, emissive);
		const lib::Span<lib::Span<Byte>> alphaMipsData             = DownloadMipsAsBC4(tempArena, alphaMipsStagedData, alpha);

		const auto cacheTextureDefinition = [](MaterialTextureDefinition& textureDefinition, const rg::RGTextureViewHandle& textureView, rhi::EFragmentFormat format, lib::Span<const lib::Span<Byte>> mipsData)
		{
			if (textureView)
			{
				textureDefinition.resolution   = textureView->GetResolution2D();
				textureDefinition.format       = format;
				textureDefinition.mipLevelsNum = static_cast<Uint32>(mipsData.size());
			}
		};

		cacheTextureDefinition(headerData.baseColorTexture, baseColor,                 rhi::EFragmentFormat::BC1_sRGB, baseColorMipsData);
		cacheTextureDefinition(headerData.metallicRoughnessTexture, metallicRoughness, rhi::EFragmentFormat::BC1_UN,   metallicRoughnessMipsData);
		cacheTextureDefinition(headerData.normalsTexture, normals,                     rhi::EFragmentFormat::BC5_UN,   normalsMipsData);
		cacheTextureDefinition(headerData.emissiveTexture, emissive,                   rhi::EFragmentFormat::BC1_sRGB, emissiveMipsData);
		cacheTextureDefinition(headerData.alphaTexture, alpha,                         rhi::EFragmentFormat::BC4_UN,   alphaMipsData);

		const auto accmulateTextureOffset = [&accumulatedDataOffset](MaterialTextureDefinition& textureDef, lib::Span<const lib::Span<Byte>> mipsData)
		{
			SPT_CHECK(mipsData.size() < MaterialTextureDefinition::s_maxMipLevelsNum);

			Uint32 mipsOffset = accumulatedDataOffset;
			for (Uint32 mipIdx = 0u; mipIdx < mipsData.size(); ++mipIdx)
			{
				const lib::Span<Byte>& mipData = mipsData[mipIdx];
				const Uint32 mipSize = static_cast<Uint32>(mipData.size());

				textureDef.mipLevels[mipIdx] = MaterialTextureMipDefinition{.dataOffset = mipsOffset, .dataSize = mipSize};

				mipsOffset            += static_cast<Uint32>(mipSize);
				accumulatedDataOffset += static_cast<Uint32>(mipSize);
			}
		};

		accmulateTextureOffset(headerData.baseColorTexture, baseColorMipsData);
		accmulateTextureOffset(headerData.metallicRoughnessTexture, metallicRoughnessMipsData);
		accmulateTextureOffset(headerData.normalsTexture, normalsMipsData);
		accmulateTextureOffset(headerData.emissiveTexture, emissiveMipsData);
		accmulateTextureOffset(headerData.alphaTexture, alphaMipsData);

		compiledData.resize(accumulatedDataOffset);

		const auto downloadMipsData = [&compiledData](const MaterialTextureDefinition& texture, lib::Span<const lib::Span<Byte>> mipsData)
		{
			for (Uint32 mipIdx = 0u; mipIdx < mipsData.size(); ++mipIdx)
			{
				const lib::Span<Byte> mipData(mipsData[mipIdx]);

				const MaterialTextureMipDefinition& mip = texture.mipLevels[mipIdx];

				SPT_CHECK(mip.dataSize == mipData.size());

				std::memcpy(compiledData.data() + mip.dataOffset, mipData.data(), mipData.size());
			}
		};

		downloadMipsData(headerData.baseColorTexture, baseColorMipsData);
		downloadMipsData(headerData.metallicRoughnessTexture, metallicRoughnessMipsData);
		downloadMipsData(headerData.normalsTexture, normalsMipsData);
		downloadMipsData(headerData.emissiveTexture, emissiveMipsData);
		downloadMipsData(headerData.alphaTexture, alphaMipsData);
	}

	std::memcpy(compiledData.data(), &headerData, sizeof(PBRMaterialDataHeader));

	return compiledData;
}

lib::DynamicArray<Byte> CompilePBRMaterial(const AssetInstance& asset, const PBRMaterialDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	const lib::Path referencePath         = asset.GetDirectoryPath();
	const lib::Path baseColorPath         = !definition.baseColorTexPath.empty() ? (referencePath / definition.baseColorTexPath) : lib::Path{};
	const lib::Path metallicRoughnessPath = !definition.metallicRoughnessTexPath.empty() ? (referencePath / definition.metallicRoughnessTexPath) : lib::Path{};
	const lib::Path normalsPath           = !definition.normalsTexPath.empty() ? (referencePath / definition.normalsTexPath) : lib::Path{};
	const lib::Path emissivePath          = !definition.emissiveTexPath.empty() ? (referencePath / definition.emissiveTexPath) : lib::Path{};

	PBRMaterialCompilationInput compilationInput;
	compilationInput.baseColorTex         = TryLoadTexture(baseColorPath);
	compilationInput.metallicRoughnessTex = TryLoadTexture(metallicRoughnessPath);
	compilationInput.normalsTex           = TryLoadTexture(normalsPath);
	compilationInput.emissiveTex          = TryLoadTexture(emissivePath);
	compilationInput.baseColorFactor      = definition.baseColorFactor;
	compilationInput.metallicFactor       = definition.metallicFactor;
	compilationInput.roughnessFactor      = definition.roughnessFactor;
	compilationInput.emissionFactor       = definition.emissionFactor;
	compilationInput.doubleSided          = definition.doubleSided;
	compilationInput.customOpacity        = definition.customOpacity;

	gfx::FlushPendingUploads();

	return CompilePBRMaterialImpl(asset, compilationInput);
}


static const tinygltf::Material* FindSourceMaterial(const rsc::GLTFModel& model, const PBRGLTFMaterialDefinition& definition)
{
	if (!definition.materialName.empty())
	{
		for (const tinygltf::Material& mat : model.materials)
		{
			if (mat.name == definition.materialName)
			{
				return &mat;
			}
		}

		return nullptr;
	}
	else
	{
		return definition.materialIdx < model.materials.size() ? &model.materials[definition.materialIdx] : nullptr;
	}
}


static rhi::EFragmentFormat GetGLTFImageFormat(const tinygltf::Image& image, Bool isSRGB)
{
	if (image.component == 1)
	{
		SPT_CHECK(!isSRGB);

		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::R8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::R16_UN_Float; 
		}
	}
	else if (image.component == 2)
	{
		SPT_CHECK(!isSRGB);

		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::RG8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::RG16_UN_Float; 
		}
	}
	else if (image.component == 3)
	{
		SPT_CHECK(!isSRGB);

		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return rhi::EFragmentFormat::RGB8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	return rhi::EFragmentFormat::RGB16_UN_Float; 
		}
	}
	else if (image.component == 4)
	{
		switch (image.pixel_type)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:		return isSRGB ? rhi::EFragmentFormat::RGBA8_sRGB_Float : rhi::EFragmentFormat::RGBA8_UN_Float;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:	SPT_CHECK(!isSRGB); return rhi::EFragmentFormat::RGBA16_UN_Float; 
		}
	}

	SPT_CHECK_NO_ENTRY();
	return rhi::EFragmentFormat::None;
}


static lib::SharedRef<rdr::Texture> CreateGLTFImage(const tinygltf::Image& image, Bool isSRGB)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIAllocationInfo allocationInfo(rhi::EMemoryUsage::GPUOnly);

	rhi::TextureDefinition textureDef;
	textureDef.resolution  = math::Vector3u(static_cast<Uint32>(image.width), static_cast<Uint32>(image.height), 1);
	textureDef.usage       = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::TransferSource);
	textureDef.format      = GetGLTFImageFormat(image, isSRGB);

	return rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(image.name), textureDef, allocationInfo);
}


lib::SharedPtr<rdr::TextureView> LoadTexture(const rsc::GLTFModel& model, int textureIdx, Bool isSRGB)
{
	const int imageIdx = textureIdx != -1 ? model.textures[textureIdx].source : -1;

	if (imageIdx == -1)
	{
		return nullptr;
	}

	const tinygltf::Image& image = model.images[imageIdx];
	const lib::SharedRef<rdr::Texture> texture = CreateGLTFImage(image, isSRGB);

	gfx::UploadDataToTexture(reinterpret_cast<const Byte*>(image.image.data()), image.image.size(), texture, rhi::ETextureAspect::Color, texture->GetResolution());

	return texture->CreateView(RENDERER_RESOURCE_NAME("GLTF Texture View"));
}


lib::DynamicArray<Byte> CompilePBRMaterial(const AssetInstance& asset, const PBRGLTFMaterialDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	const lib::Path referencePath  = asset.GetDirectoryPath();
	const lib::Path gltfSourcePath = !definition.gltfSourcePath.empty() ? (referencePath / definition.gltfSourcePath) : lib::Path{};
	const lib::String gltfSourcePathAsString = gltfSourcePath.generic_string();

	const std::optional<rsc::GLTFModel>& gltfModel = asset.GetOwningSystem().GetCompilationInputData<std::optional<rsc::GLTFModel>>(gltfSourcePathAsString,
			[&gltfSourcePathAsString]()
			{
				return rsc::LoadGLTFModel(gltfSourcePathAsString);
			});

	if (!gltfModel)
	{
		return {};
	}

	const tinygltf::Material* srcMat = FindSourceMaterial(*gltfModel, definition);
	if (!srcMat)
	{
		return {};
	}

	const tinygltf::PbrMetallicRoughness& srcMatPBR = srcMat->pbrMetallicRoughness;

	float emissiveStrength = 1.f;
	const auto emissiveStrengthIter = srcMat->extensions.find("KHR_materials_emissive_strength");
	if (emissiveStrengthIter != srcMat->extensions.end())
	{
		emissiveStrength = static_cast<Real32>(emissiveStrengthIter->second.Get("emissiveStrength").GetNumberAsDouble());
	}

	const Bool isMasked = srcMat->alphaMode == "MASK";

	PBRMaterialCompilationInput compilationInput;
	compilationInput.baseColorTex         = LoadTexture(*gltfModel, srcMatPBR.baseColorTexture.index, false);
	compilationInput.metallicRoughnessTex = LoadTexture(*gltfModel, srcMatPBR.metallicRoughnessTexture.index, false);
	compilationInput.normalsTex           = LoadTexture(*gltfModel, srcMat->normalTexture.index, false);
	compilationInput.emissiveTex          = LoadTexture(*gltfModel, srcMat->emissiveTexture.index, false);
	compilationInput.baseColorFactor      = math::Map<const math::Vector3d>(srcMatPBR.baseColorFactor.data()).cast<Real32>();;
	compilationInput.metallicFactor       = static_cast<Real32>(srcMatPBR.metallicFactor);
	compilationInput.roughnessFactor      = static_cast<Real32>(srcMatPBR.roughnessFactor);
	compilationInput.emissionFactor       = math::Map<const math::Vector3d>(srcMat->emissiveFactor.data()).cast<Real32>() * emissiveStrength;
	compilationInput.doubleSided          = srcMat->doubleSided;
	compilationInput.customOpacity        = isMasked;

	gfx::FlushPendingUploads();

	return CompilePBRMaterialImpl(asset, compilationInput);
}

} // spt::as::material_compiler
