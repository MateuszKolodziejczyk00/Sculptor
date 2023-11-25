#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICore/RHIBufferTypes.h"
#include "RHIBridge/RHIFwd.h"
#include "RendererUtils.h"
#include "UIContext.h"
#include "Types/DescriptorSetWriter.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Shaders/ShaderTypes.h"
#include "Pipelines/PipelineState.h"
#include "Common/ShaderCompilationInput.h"


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
struct BLASDefinition;
struct TLASDefinition;
struct QueryPoolDefinition;
struct RHIMemoryPoolDefinition;
} // spt::rhi


namespace spt::smd
{
class ShaderMetaData;
} // spt::smd


namespace spt::rdr
{

class RenderContext;
class Window;
class Event;
class AllocationDefinition;
class Buffer;
class Texture;
class TextureView;
class GPUMemoryPool;
class Semaphore;
class CommandBuffer;
class Shader;
class GraphicsPipeline;
class ComputePipeline;
class Sampler;
class BottomLevelAS;
class TopLevelAS;
class QueryPool;


class RENDERER_CORE_API ResourcesManager
{
public:

	SPT_NODISCARD static lib::SharedRef<RenderContext>		CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef);
	
	SPT_NODISCARD static lib::SharedRef<Window>				CreateWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo);

	SPT_NODISCARD static lib::SharedRef<Event>				CreateEvent(const RendererResourceName& name, const rhi::EventDefinition& definition);
	
	SPT_NODISCARD static lib::SharedRef<Buffer>				CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition);
	SPT_NODISCARD static lib::SharedRef<Buffer>				CreateBuffer(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance);

	SPT_NODISCARD static lib::SharedRef<Texture>			CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationInfo);
	SPT_NODISCARD static lib::SharedRef<TextureView>		CreateTextureView(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationInfo);

	SPT_NODISCARD static lib::SharedRef<GPUMemoryPool>		CreateGPUMemoryPool(const RendererResourceName& name, const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);

	SPT_NODISCARD static lib::SharedRef<Semaphore>			CreateRenderSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	SPT_NODISCARD static lib::SharedRef<CommandBuffer>		CreateCommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition); 

	SPT_NODISCARD static ShaderID							CreateShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings = sc::ShaderCompilationSettings(), EShaderFlags flags = EShaderFlags::None);
	SPT_NODISCARD static lib::SharedRef<Shader>				GetShaderObject(ShaderID shaderID);

	SPT_NODISCARD static PipelineStateID					CreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef);
	SPT_NODISCARD static PipelineStateID					CreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader);
	SPT_NODISCARD static PipelineStateID					CreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef);
	
	SPT_NODISCARD static lib::SharedRef<BottomLevelAS>		CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition);
	SPT_NODISCARD static lib::SharedRef<TopLevelAS>			CreateTLAS(const RendererResourceName& name, const rhi::TLASDefinition& definition);

	SPT_NODISCARD static DescriptorSetWriter				CreateDescriptorSetWriter();

	SPT_NODISCARD static lib::SharedRef<Sampler>			CreateSampler(const rhi::SamplerDefinition& def);

	template<typename TDSState>
	SPT_NODISCARD static lib::MTHandle<TDSState>			CreateDescriptorSetState(const RendererResourceName& name, rdr::EDescriptorSetStateFlags flags = rdr::EDescriptorSetStateFlags::Default);
	
	SPT_NODISCARD static lib::SharedRef<QueryPool>			CreateQueryPool(const rhi::QueryPoolDefinition& def);

private:

	// block creating instance
	ResourcesManager() = default;
};


template<typename TDSState>
lib::MTHandle<TDSState> ResourcesManager::CreateDescriptorSetState(const RendererResourceName& name, rdr::EDescriptorSetStateFlags flags /*= rdr::EDescriptorSetStateFlags::Default*/)
{
	return lib::MTHandle<TDSState>(new TDSState(name, flags));
}

} // spt::rdr