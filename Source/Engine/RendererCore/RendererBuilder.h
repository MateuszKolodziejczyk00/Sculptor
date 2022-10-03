#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "RHIBridge/RHIFwd.h"
#include "RendererUtils.h"
#include "Types/Barrier.h"
#include "UIContext.h"
#include "Types/DescriptorSetWriter.h"


namespace spt::rhi
{
struct ContextDefinition;
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

class Context;
class Window;
class UIBackend;
class Buffer;
class Texture;
class Semaphore;
class CommandBuffer;
class Shader;
class GraphicsPipeline;
class ComputePipeline;


class RENDERER_CORE_API RendererBuilder
{
public:

	SPT_NODISCARD static lib::SharedRef<Context>			CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef);
	
	SPT_NODISCARD static lib::SharedRef<Window>				CreateWindow(lib::StringView name, math::Vector2u resolution);

	SPT_NODISCARD static lib::SharedRef<UIBackend>			CreateUIBackend(ui::UIContext context, const lib::SharedRef<Window>& window);
	
	SPT_NODISCARD static lib::SharedRef<Buffer>				CreateBuffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo);
	SPT_NODISCARD static lib::SharedRef<Texture>			CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);

	SPT_NODISCARD static lib::SharedRef<Semaphore>			CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	SPT_NODISCARD static lib::SharedRef<CommandBuffer>		CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition); 

	SPT_NODISCARD static Barrier							CreateBarrier();

	SPT_NODISCARD static lib::SharedRef<Shader>				CreateShader(const RendererResourceName& name, const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions, const lib::SharedRef<smd::ShaderMetaData>& metaData);

	SPT_NODISCARD static lib::SharedRef<GraphicsPipeline>	CreateGraphicsPipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader, const rhi::GraphicsPipelineDefinition pipelineDef);
	SPT_NODISCARD static lib::SharedRef<ComputePipeline>	CreateComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader);

	SPT_NODISCARD static DescriptorSetWriter				CreateDescriptorSetWriter();

private:

	// block creating instance
	RendererBuilder() = default;
};

} // spt::rdr