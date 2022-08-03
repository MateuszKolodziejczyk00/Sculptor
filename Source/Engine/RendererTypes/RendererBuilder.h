#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIFwd.h"
#include "RendererUtils.h"
#include "Types/Barrier.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
struct TextureDefinition;
struct SemaphoreDefinition;
struct CommandBufferDefinition;
}


namespace spt::renderer
{

class Buffer;
class Texture;
class Window;
class Semaphore;
class CommandBuffer;


class RENDERER_TYPES_API RendererBuilder
{
public:

	static lib::SharedPtr<Window>				CreateWindow(lib::StringView name, math::Vector2u resolution);

	static lib::SharedPtr<Buffer>				CreateBuffer(const RendererResourceName& name, Uint64 size, Flags32 bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	static lib::SharedPtr<Texture>				CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);

	static lib::SharedPtr<Semaphore>			CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	static lib::SharedPtr<CommandBuffer>		CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition);

	static Barrier								CreateBarrier();
};

}