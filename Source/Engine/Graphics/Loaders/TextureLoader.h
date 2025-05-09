#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "Job.h"


namespace spt::rdr
{
class Texture;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::gfx
{

struct TextureLoadParams
{
	static constexpr rhi::ETextureUsage defaultUsage = lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferSource, rhi::ETextureUsage::TransferDest);

	rhi::ETextureUsage                  usage       = defaultUsage;
	rhi::EMemoryUsage                   memoryUsage = rhi::EMemoryUsage::GPUOnly;
	std::optional<rhi::ETextureTiling>  forceTiling = std::nullopt;
};


class GRAPHICS_API TextureLoader
{
public:

	static lib::SharedPtr<rdr::Texture> LoadTexture(lib::StringView path, rhi::ETextureUsage usage = TextureLoadParams::defaultUsage, rhi::EMemoryUsage memoryUsage = rhi::EMemoryUsage::GPUOnly);

	static lib::SharedPtr<rdr::Texture> LoadTexture(lib::StringView path, const TextureLoadParams& params);

private:

	TextureLoader() = delete;
};


class GRAPHICS_API TextureWriter
{
public:

	struct Texture2DData
	{
		lib::Span<Byte>      data;
		math::Vector2u       resolution = math::Vector3u::Constant(0u);
		rhi::EFragmentFormat format     = rhi::EFragmentFormat::None;
	};

	static Bool SaveTexture(lib::SharedRef<rdr::Texture> texture, const lib::String& path);

	static js::JobWithResult<Bool> SaveTexture(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle textureView, lib::String path);

private:

	TextureWriter() = delete;
};

} // spt::gfx