#include "RendererBuilder.h"
#include "Types/Buffer.h"
#include "Types/Texture.h"


namespace spt::renderer
{

lib::SharedPtr<Buffer> RendererBuilder::CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Buffer>(name, size, bufferUsage, allocationInfo);
}

lib::SharedPtr<Texture> RendererBuilder::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Texture>(name, textureDefinition, allocationInfo);
}

}
