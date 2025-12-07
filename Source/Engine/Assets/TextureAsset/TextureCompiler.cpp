#include "TextureCompiler.h"
#include "Types/Texture.h"
#include "Loaders/TextureLoader.h"
#include "AssetsSystem.h"

SPT_DEFINE_LOG_CATEGORY(TextureCompiler, true);


namespace spt::as
{

std::optional<TextureCompilationResult> TextureCompiler::CompileTexture(const TextureAsset& owningAsset, const TextureSourceDefinition& textureSource)
{
	SPT_PROFILER_FUNCTION();

	const gfx::TextureLoadParams loadParams
	{
		.memoryUsage = rhi::EMemoryUsage::CPUOnly,
		.forceTiling = rhi::ETextureTiling::Linear
	};

	const lib::Path textureSourcePath = (owningAsset.GetDirectoryPath() / textureSource.path);

	const lib::SharedPtr<rdr::Texture> loadedTexture = gfx::TextureLoader::LoadTexture(textureSourcePath.generic_string(), loadParams);

	if (!loadedTexture)
	{
		SPT_LOG_ERROR(TextureCompiler, "Failed to load texture from path: {}", textureSourcePath.generic_string());
		return std::nullopt;
	}

	const rhi::RHITexture& rhiTexture = loadedTexture->GetRHI();

	SPT_CHECK_MSG(rhiTexture.GetDefinition().arrayLayers == 1u, "Array layers > 1 are not supported yet");

	TextureCompilationResult result;
	result.compiledTexture.definition.resolution   = rhiTexture.GetResolution();
	result.compiledTexture.definition.format       = rhiTexture.GetFormat();
	result.compiledTexture.definition.mipLevelsNum = rhiTexture.GetDefinition().mipLevels;

	Uint32 accumulatedTextureDataSize = 0u;
	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < rhiTexture.GetDefinition().mipLevels; ++mipLevelIdx)
	{
		const Uint32 mipSize = static_cast<Uint32>(rhiTexture.GetMipSize(mipLevelIdx));
		result.compiledTexture.mips.emplace_back(CompiledMip{ .offset = accumulatedTextureDataSize, .size = mipSize });

		accumulatedTextureDataSize += mipSize;
	}

	result.textureData.resize(accumulatedTextureDataSize);

	rhi::RHIMappedTexture mappedTexture(rhiTexture);

	Uint32 copyOffset = 0u;
	for (Uint32 mipLevelIdx = 0u; mipLevelIdx < rhiTexture.GetDefinition().mipLevels; ++mipLevelIdx)
	{
		SPT_CHECK(copyOffset == result.compiledTexture.mips[mipLevelIdx].offset);

		const rhi::RHIMappedSurface mappedSurface = mappedTexture.GetSurface(mipLevelIdx, 0u);
		const lib::Span<const Byte> mipData = mappedSurface.GetMipData();

		const lib::Span<Byte> ddcMipData(result.textureData.data() + copyOffset, mipData.size());
		std::memcpy(ddcMipData.data(), mipData.data(), mipData.size());

		copyOffset += static_cast<Uint32>(mipData.size());
	}

	return result;
}

} // spt::as
