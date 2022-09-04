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
struct GraphicsPipelineDefinition;
struct ShaderModuleDefinition;
} // spt::rhi


namespace spt::smd
{
class ShaderMetaData;
} // spt::smd


namespace spt::rdr
{

class Window;
class UIBackend;
class Buffer;
class Texture;
class Semaphore;
class CommandBuffer;
class Shader;
class GraphicsPipeline;
class ComputePipeline;


class RENDERER_TYPES_API RendererBuilder
{
public:

	SPT_NODISCARD static lib::SharedPtr<Window>				CreateWindow(lib::StringView name, math::Vector2u resolution);

	SPT_NODISCARD static lib::SharedPtr<UIBackend>			CreateUIBackend(ui::UIContext context, const lib::SharedPtr<Window>& window);
	
	SPT_NODISCARD static lib::SharedPtr<Buffer>				CreateBuffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	SPT_NODISCARD static lib::SharedPtr<Texture>			CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);

	SPT_NODISCARD static lib::SharedPtr<Semaphore>			CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	SPT_NODISCARD static lib::SharedPtr<CommandBuffer>		CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition); 

	SPT_NODISCARD static Barrier							CreateBarrier();

	SPT_NODISCARD static lib::SharedPtr<Shader>				CreateShader(const RendererResourceName& name, const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions, const lib::SharedPtr<smd::ShaderMetaData>& metaData);

	SPT_NODISCARD static lib::SharedPtr<GraphicsPipeline>	CreateGraphicsPipeline(const RendererResourceName& name, const lib::SharedPtr<Shader>& shader, const rhi::GraphicsPipelineDefinition pipelineDef);
	SPT_NODISCARD static lib::SharedPtr<ComputePipeline>	CreateComputePipeline(const RendererResourceName& name, const lib::SharedPtr<Shader>& shader);

private:

	// block creating instance
	RendererBuilder() = default;
};

} // spt::rdr