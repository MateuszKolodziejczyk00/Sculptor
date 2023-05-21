#include "TextureLoader.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "Transfers/UploadsManager.h"

#pragma warning(push)
#pragma warning(disable: 4244)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#pragma warning(pop)

namespace spt::gfx
{

lib::SharedPtr<rdr::Texture> TextureLoader::LoadTexture(lib::StringView path, rhi::ETextureUsage usage, rhi::EMemoryUsage memoryUsage /*= rhi::EMemoryUsage::GPUOnly*/, std::optional<rhi::EFragmentFormat> forceFormat /*= std::nullopt*/)
{
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

	UploadsManager::Get().EnqueUploadToTexture(reinterpret_cast<const Byte*>(imageData), imageSize, texture, aspect, textureDefinition.resolution);

	stbi_image_free(imageData);

	return texture;
}

} // spt::gfx
