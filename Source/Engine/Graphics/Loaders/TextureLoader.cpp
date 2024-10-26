#include "TextureLoader.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "Transfers/UploadsManager.h"
#include "RenderGraphBuilder.h"
#include "FileSystem/File.h"

#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma warning(pop)

namespace spt::gfx
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureLoader =================================================================================

lib::SharedPtr<rdr::Texture> TextureLoader::LoadTexture(lib::StringView path, rhi::ETextureUsage usage, rhi::EMemoryUsage memoryUsage /*= rhi::EMemoryUsage::GPUOnly*/, std::optional<rhi::EFragmentFormat> forceFormat /*= std::nullopt*/)
{
	SPT_PROFILER_FUNCTION();

	const Bool isHDR = stbi_is_hdr(path.data());

	int width;
	int height;
	int components;

	const int requiredComonents = 4;

	stbi_uc* imageData = stbi_load(path.data(), OUT &width, OUT &height, OUT &components, requiredComonents);

	if (!imageData)
	{
		return nullptr;
	}

	SPT_CHECK(width > 0);
	SPT_CHECK(height > 0);

	rhi::TextureDefinition textureDefinition;
	textureDefinition.resolution	= math::Vector3u(static_cast<Uint32>(width), static_cast<Uint32>(height), 1u);
	textureDefinition.usage			= usage;
	textureDefinition.format		= forceFormat.value_or(isHDR ? rhi::EFragmentFormat::RGBA32_S_Float : rhi::EFragmentFormat::RGBA8_UN_Float);

	const lib::SharedRef<rdr::Texture> texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(path), textureDefinition, memoryUsage);

	const Uint64 imageSize = static_cast<Uint64>(width) * static_cast<Uint64>(height) * requiredComonents * (isHDR ? sizeof(Real32) : 1u);
	const rhi::ETextureAspect aspect = rhi::GetFullAspectForFormat(textureDefinition.format);

	UploadsManager::Get().EnqueUploadToTexture(reinterpret_cast<const Byte*>(imageData), imageSize, texture, aspect, textureDefinition.resolution.AsVector());

	stbi_image_free(imageData);

	return texture;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureWriter =================================================================================

Bool TextureWriter::SaveTexture(const Texture2DData& textureData, const lib::String& path)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(textureData.format == rhi::EFragmentFormat::RGBA8_UN_Float || textureData.format == rhi::EFragmentFormat::RGBA32_S_Float);

	const Bool isHDR = textureData.format == rhi::EFragmentFormat::RGBA32_S_Float;
	const int components = 4;

	const Uint64 fragmentSize = rhi::GetFragmentSize(textureData.format);

	const Uint64 imageSize = static_cast<Uint64>(textureData.resolution.x()) * static_cast<Uint64>(textureData.resolution.y()) * fragmentSize;
	SPT_CHECK(textureData.data.size() == imageSize);

	const math::Vector2i resolution = textureData.resolution.cast<int>();

	int result = 0;

	std::ofstream stream = lib::File::OpenOutputStream(path, lib::Flags(lib::EFileOpenFlags::ForceCreate, lib::EFileOpenFlags::DiscardContent, lib::EFileOpenFlags::Binary));
	SPT_CHECK(stream.is_open());

	const auto writer = [](void* context, void* data, int size)
	{
		std::ostream& stream = *reinterpret_cast<std::ostream*>(context);
		stream.write(reinterpret_cast<const char*>(data), size);
	};

	if (isHDR)
	{
		result = stbi_write_hdr_to_func(writer, &stream, resolution.x(), resolution.y(), components, reinterpret_cast<const Real32*>(textureData.data.data()));
	}
	else
	{
		const int rowStride = static_cast<int>(textureData.resolution.x() * fragmentSize);
		result = stbi_write_png_to_func(writer, &stream, resolution.x(), resolution.y(), components, textureData.data.data(), rowStride);
	}

	stream.close();

	return result != 0;
}

js::JobWithResult<Bool> TextureWriter::SaveTexture(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle textureView, lib::String path)
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::Buffer> textureData = graphBuilder.DownloadTexture(RG_DEBUG_NAME("Texture Download"), textureView);

	const rhi::EFragmentFormat format = textureView->GetTextureDefinition().format;
	const math::Vector2u resolution   = textureView->GetResolution2D();

	return js::Launch(SPT_GENERIC_JOB_NAME,
					  [textureData, format, resolution, filePath = std::move(path)]
					  {
						  rhi::RHIMappedByteBuffer mappedBuffer(textureData->GetRHI());

						  const Texture2DData texture2DData
						  {
							  mappedBuffer.GetSpan(),
							  resolution,
							  format
						  };

						  return TextureWriter::SaveTexture(texture2DData, filePath);
					  },
					  js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
}

} // spt::gfx
