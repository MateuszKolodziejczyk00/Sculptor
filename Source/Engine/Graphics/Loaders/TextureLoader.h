#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RHICore/RHIAllocationTypes.h"


namespace spt::rdr
{
class Texture;
} // spt::rdr


namespace spt::gfx
{

class GRAPHICS_API TextureLoader
{
public:

	static lib::SharedPtr<rdr::Texture> LoadTexture(lib::StringView path, rhi::ETextureUsage usage, rhi::EMemoryUsage memoryUsage = rhi::EMemoryUsage::GPUOnly, std::optional<rhi::EFragmentFormat> forceFormat = std::nullopt);

private:

	TextureLoader() = delete;
};

} // spt::gfx