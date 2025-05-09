#include "TextureCompiler.h"
#include "Types/Texture.h"
#include "Loaders/TextureLoader.h"
#include "AssetsSystem.h"

namespace spt::as
{

CompiledTextureData TextureCompiler::CompileTexture(const TextureSourceDefinition& textureSource, const TextureAsset& owningAsset, const DDC& ddc)
{
	SPT_PROFILER_FUNCTION();

	const gfx::TextureLoadParams loadParams
	{
		.memoryUsage = rhi::EMemoryUsage::CPUOnly,
		.forceTiling = rhi::ETextureTiling::Linear
	};

	const lib::Path textureSourcePath = (owningAsset.GetOwningSystem().GetContentPath() / textureSource.path).generic_string();

	const lib::SharedPtr<rdr::Texture> loadedTexture = gfx::TextureLoader::LoadTexture(textureSourcePath.generic_string(), loadParams);

	const rhi::RHITexture& rhiTexture = loadedTexture->GetRHI();

	SPT_CHECK_MSG(loadedTexture, "Failed to load texture");

	SPT_CHECK_MSG(rhiTexture.GetDefinition().arrayLayers == 1u, "Array layers > 1 are not supported yet");

	CompiledTextureData compiledTexture;

	Uint32 accumulatedTextureDataSize = 0u;
	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < rhiTexture.GetDefinition().mipLevels; ++mipLevelIdx)
	{
		const Uint32 mipSize = static_cast<Uint32>(rhiTexture.GetMipSize(mipLevelIdx));
		compiledTexture.texture.mips.emplace_back(CompiledMip{ .offset = accumulatedTextureDataSize, .size = mipSize });

		accumulatedTextureDataSize += mipSize;
	}

	const DDCResourceHandle ddcResourceHandle =  ddc.CreateDerivedData(accumulatedTextureDataSize);

	rhi::RHIMappedTexture mappedTexture(rhiTexture);

	Uint32 copyOffset = 0u;
	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < rhiTexture.GetDefinition().mipLevels; ++mipLevelIdx)
	{
		SPT_CHECK(copyOffset == compiledTexture.texture.mips[mipLevelIdx].offset);

		const rhi::RHIMappedSurface mappedSurface = mappedTexture.GetSurface(mipLevelIdx, 0u);
		const lib::Span<const Byte> mipData = mappedSurface.GetMipData();

		const lib::Span<Byte> ddcMipData = ddcResourceHandle.GetMutableSpan().subspan(copyOffset, mipData.size());
		std::memcpy(ddcMipData.data(), mipData.data(), mipData.size());

		copyOffset += static_cast<Uint32>(mipData.size());
	}

	compiledTexture.derivedDataKey = ddcResourceHandle.GetKey();

	compiledTexture.texture.definition.resolution   = rhiTexture.GetResolution();
	compiledTexture.texture.definition.format       = rhiTexture.GetFormat();
	compiledTexture.texture.definition.mipLevelsNum = rhiTexture.GetDefinition().mipLevels;

	return compiledTexture;
}

} // spt::as
