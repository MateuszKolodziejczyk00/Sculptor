#include "RendererBuilder.h"
#include "Types/Buffer.h"
#include "Types/Texture.h"
#include "Types/Window.h"


namespace spt::renderer
{

lib::SharedPtr<Window> RendererBuilder::CreateWindow(lib::StringView name, math::Vector2u resolution)
{
	return std::make_shared<Window>(name, resolution);
}

lib::SharedPtr<Buffer> RendererBuilder::CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Buffer>(name, size, bufferUsage, allocationInfo);
}

lib::SharedPtr<Texture> RendererBuilder::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Texture>(name, textureDefinition, allocationInfo);
}

}
