#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIBufferTypes.h"
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
struct ShaderModuleDefinition;
}


namespace spt::rdr
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

	SPT_NODISCARD static lib::SharedPtr<Window>			CreateWindow(lib::StringView name, math::Vector2u resolution);

	SPT_NODISCARD static lib::SharedPtr<UIBackend>		CreateUIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window);
	
	SPT_NODISCARD static lib::SharedPtr<Buffer>			CreateBuffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	SPT_NODISCARD static lib::SharedPtr<Texture>		CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);

	SPT_NODISCARD static lib::SharedPtr<Semaphore>		CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	SPT_NODISCARD static lib::SharedPtr<CommandBuffer>	CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition); 
	SPT_NODISCARD static Barrier						CreateBarrier();
};

}