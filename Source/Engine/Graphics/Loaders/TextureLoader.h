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

class GRAPHICS_API TextureLoader
{
public:

	static lib::SharedPtr<rdr::Texture> LoadTexture(lib::StringView path, rhi::ETextureUsage usage, rhi::EMemoryUsage memoryUsage = rhi::EMemoryUsage::GPUOnly, std::optional<rhi::EFragmentFormat> forceFormat = std::nullopt);

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


	static Bool SaveTexture(const Texture2DData& textureData, const lib::String& path);

	static js::JobWithResult<Bool> SaveTexture(rg::RenderGraphBuilder& graphBuilder, rg::RGTextureViewHandle textureView, lib::String path);

private:

	TextureWriter() = delete;
};

} // spt::gfx