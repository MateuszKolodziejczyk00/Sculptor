#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIFwd.h"
#include "RendererUtils.h"
#include "Types/Barrier.h"
#include "UITypes.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
struct TextureDefinition;
struct SemaphoreDefinition;
struct CommandBufferDefinition;
}


namespace spt::renderer
{

class Window;
class UIBackend;
class Buffer;
class Texture;
class Semaphore;
class CommandBuffer;


class RENDERER_TYPES_API RendererBuilder
{
public:

	static lib::SharedPtr<Window>				CreateWindow(lib::StringView name, math::Vector2u resolution);

	static lib::SharedPtr<UIBackend>			CreateUIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window);
	
	static lib::SharedPtr<Buffer>				CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	static lib::SharedPtr<Texture>				CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);

	static lib::SharedPtr<Semaphore>			CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	static lib::SharedPtr<CommandBuffer>		CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition);

	static Barrier								CreateBarrier();
};

}