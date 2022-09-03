#include "RendererBuilder.h"
#include "Types/Buffer.h"
#include "Types/Texture.h"
#include "Types/Window.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/UIBackend.h"


namespace spt::rdr
{

lib::SharedPtr<Window> RendererBuilder::CreateWindow(lib::StringView name, math::Vector2u resolution)
{
	return std::make_shared<Window>(name, resolution);
}

lib::SharedPtr<UIBackend> RendererBuilder::CreateUIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window)
{
	return std::make_shared<UIBackend>(context, window);
}

lib::SharedPtr<Buffer> RendererBuilder::CreateBuffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Buffer>(name, size, bufferUsage, allocationInfo);
}

lib::SharedPtr<Texture> RendererBuilder::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return std::make_shared<Texture>(name, textureDefinition, allocationInfo);
}

lib::SharedPtr<Semaphore> RendererBuilder::CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	return std::make_shared<Semaphore>(name, definition);
}

lib::SharedPtr<CommandBuffer> RendererBuilder::CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition)
{
	return std::make_shared<CommandBuffer>(name, definition);
}

Barrier RendererBuilder::CreateBarrier()
{
	return Barrier();
}

}
