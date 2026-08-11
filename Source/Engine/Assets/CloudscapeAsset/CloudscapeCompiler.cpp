#include "CloudscapeCompiler.h"
#include "CloudscapeAsset.h"
#include "Loaders/TextureLoader.h"


SPT_DEFINE_LOG_CATEGORY(CloudscapeCompiler, true);

namespace spt::as::cloudscape_compiler
{

static void AppendDefaultWeatherMap(CompiledCloudscapeHeader& header, CloudscapeCompilationResult& result)
{
	const math::Vector2u resolution = math::Vector2u(2048u, 2048u);
	constexpr rhi::EFragmentFormat format = rhi::EFragmentFormat::RGBA8_UN_Float;
	const Uint32 dataSize = resolution.x() * resolution.y() * rhi::GetFragmentInfo(format).bytesPerBlock;

	header.weatherMap.resolution = resolution;
	header.weatherMap.format     = format;
	header.weatherMap.dataOffset = static_cast<Uint32>(result.blob.size());
	header.weatherMap.dataSize   = dataSize;

	result.blob.resize(result.blob.size() + dataSize, Byte(0u));
}


std::optional<CloudscapeCompilationResult> CompileCloudscape(const AssetInstance& asset, const CloudscapeAssetDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	CloudscapeCompilationResult result;

	CompiledCloudscapeHeader header{};
	result.blob.resize(sizeof(CompiledCloudscapeHeader));

	Bool loadedWeatherMapFromTexture = false;
	if (!definition.weatherMapTex.empty())
	{
		lib::MemoryArena tempArena("CloudscapeCompilationTempArena", 256u * 1024u, 64u * 1024u * 1024u);

		const lib::Path weatherMapPath = asset.GetDirectoryPath() / definition.weatherMapTex;
		const gfx::LoadedTextureData weatherMapTex = gfx::TextureLoader::LoadTextureData(weatherMapPath.generic_string(), tempArena);
		if (weatherMapTex.IsValid())
		{
			header.weatherMap.resolution = weatherMapTex.resolution.head<2>();
			header.weatherMap.format     = weatherMapTex.format;
			header.weatherMap.dataOffset = static_cast<Uint32>(result.blob.size());
			header.weatherMap.dataSize   = static_cast<Uint32>(weatherMapTex.data.size());

			result.blob.insert(result.blob.end(), weatherMapTex.data.begin(), weatherMapTex.data.end());

			loadedWeatherMapFromTexture = true;
		}
		else
		{
			SPT_LOG_ERROR(CloudscapeCompiler, "Failed to load cloudscape weather map from path: {}", weatherMapPath.generic_string());
		}
	}

	if (!loadedWeatherMapFromTexture)
	{
		AppendDefaultWeatherMap(header, result);
	}

	std::memcpy(result.blob.data(), &header, sizeof(header));

	return result;
}

} // spt::as::cloudscape_compiler
