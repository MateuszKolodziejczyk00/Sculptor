#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIBufferTypes.h"
#include "RHIBridge/RHIFwd.h"
#include "RendererUtils.h"
#include "UIContext.h"
#include "Types/DescriptorSetWriter.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Shaders/ShaderTypes.h"
#include "Pipelines/PipelineState.h"


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
struct SamplerDefinition;
struct EventDefinition;
struct RHIWindowInitializationInfo;
} // spt::rhi


namespace spt::smd
{
class ShaderMetaData;
} // spt::smd


namespace spt::sc
{
class ShaderCompilationSettings;
} // spt::sc


namespace spt::rdr
{

class RenderContext;
class Window;
class Event;
class Buffer;
class Texture;
class Semaphore;
class CommandBuffer;
class Shader;
class GraphicsPipeline;
class ComputePipeline;
class Sampler;


class RENDERER_CORE_API ResourcesManager
{
public:

	SPT_NODISCARD static lib::SharedRef<RenderContext>		CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef);
	
	SPT_NODISCARD static lib::SharedRef<Window>				CreateWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo);

	SPT_NODISCARD static lib::SharedRef<Event>				CreateEvent(const RendererResourceName& name, const rhi::EventDefinition& definition);
	
	SPT_NODISCARD static lib::SharedRef<Buffer>				CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	SPT_NODISCARD static lib::SharedRef<Texture>			CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo);

	SPT_NODISCARD static lib::SharedRef<Semaphore>			CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	SPT_NODISCARD static lib::SharedRef<CommandBuffer>		CreateCommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition); 

	SPT_NODISCARD static ShaderID							CreateShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags = EShaderFlags::None);
	SPT_NODISCARD static lib::SharedRef<Shader>				GetShaderObject(ShaderID shaderID);

	SPT_NODISCARD static PipelineStateID					CreateGfxPipeline(const RendererResourceName& nameInNotCached, const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader);
	SPT_NODISCARD static PipelineStateID					CreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader);
	
	SPT_NODISCARD static lib::SharedRef<GraphicsPipeline>	GetGraphicsPipeline(PipelineStateID id);
	SPT_NODISCARD static lib::SharedRef<ComputePipeline>	GetComputePipeline(PipelineStateID id);

	SPT_NODISCARD static DescriptorSetWriter				CreateDescriptorSetWriter();

	SPT_NODISCARD static lib::SharedRef<Sampler>			CreateSampler(const rhi::SamplerDefinition& def);

	template<typename TDSState>
	SPT_NODISCARD static lib::SharedRef<TDSState>			CreateDescriptorSetState(const RendererResourceName& name, rdr::EDescriptorSetStateFlags flags);

private:

	// block creating instance
	ResourcesManager() = default;
};


template<typename TDSState>
lib::SharedRef<TDSState> ResourcesManager::CreateDescriptorSetState(const RendererResourceName& name, rdr::EDescriptorSetStateFlags flags)
{
	return lib::MakeShared<TDSState>(name, flags);
}

} // spt::rdr