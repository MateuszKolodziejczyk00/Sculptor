#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RendererUtils.h"
#include "RHICore/RHIBufferTypes.h"
#include "RHIBridge/RHIFwd.h"
#include "RendererUtils.h"
#include "UIContext.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Shaders/ShaderTypes.h"
#include "Pipelines/PipelineState.h"
#include "Common/ShaderCompilationInput.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHICore/RHIRenderContextTypes.h"
#include "Types/Buffer.h"


namespace spt::rhi
{
struct ContextDefinition;
struct RHIAllocationInfo;
struct TextureDefinition;
struct SemaphoreDefinition;
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
struct DescriptorSetDefinition;
struct DescriptorHeapDefinition;
} // spt::rhi


namespace spt::smd
{
class ShaderMetaData;
} // spt::smd


namespace spt::rdr
{

struct DescriptorSetStateParams;
class RenderContext;
class CommandRecorder;
class Window;
class GPUEvent;
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
class DescriptorSetLayout;
class DescriptorSetStackAllocator;
class Pipeline;
class DescriptorHeap;


class RENDERER_CORE_API ResourcesManager
{
public:

	SPT_NODISCARD static lib::SharedRef<RenderContext> CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef = rhi::ContextDefinition());

	SPT_NODISCARD static lib::UniquePtr<CommandRecorder> CreateCommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage = rhi::CommandBufferUsageDefinition());
	
	SPT_NODISCARD static lib::SharedRef<Window> CreateWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo);

	SPT_NODISCARD static lib::SharedRef<GPUEvent> CreateGPUEvent(const RendererResourceName& name, const rhi::EventDefinition& definition);
	
	SPT_NODISCARD static lib::SharedRef<Buffer> CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition, BufferViewDescriptorsAllocation descriptorsAllocation = BufferViewDescriptorsAllocation{});
	SPT_NODISCARD static lib::SharedRef<Buffer> CreateBuffer(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance);

	SPT_NODISCARD static lib::SharedRef<Texture>     CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationInfo);
	SPT_NODISCARD static lib::SharedRef<TextureView> CreateTextureView(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationInfo);

	SPT_NODISCARD static lib::SharedRef<GPUMemoryPool> CreateGPUMemoryPool(const RendererResourceName& name, const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);

	SPT_NODISCARD static lib::SharedRef<Semaphore> CreateRenderSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);

	SPT_NODISCARD static lib::SharedRef<CommandBuffer> CreateCommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition); 

	SPT_NODISCARD static ShaderID               GenerateShaderID(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings = sc::ShaderCompilationSettings(), EShaderFlags flags = EShaderFlags::None);
	static ShaderID                             CreateShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings = sc::ShaderCompilationSettings(), EShaderFlags flags = EShaderFlags::None);
	SPT_NODISCARD static lib::SharedRef<Shader> GetShaderObject(ShaderID shaderID);


	SPT_NODISCARD static PipelineStateID GeneratePipelineID(const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef);
	SPT_NODISCARD static PipelineStateID GeneratePipelineID(const ShaderID& shader);
	SPT_NODISCARD static PipelineStateID GeneratePipelineID(const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef);

	static PipelineStateID          CreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef);
	static PipelineStateID          CreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader);
	static PipelineStateID          CreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef);
	SPT_NODISCARD static lib::SharedPtr<Pipeline> GetPipelineObject(PipelineStateID id);
	
	SPT_NODISCARD static lib::SharedRef<BottomLevelAS> CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition);
	SPT_NODISCARD static lib::SharedRef<TopLevelAS>    CreateTLAS(const RendererResourceName& name, const rhi::TLASDefinition& definition);

	SPT_NODISCARD static lib::SharedRef<Sampler> CreateSampler(const rhi::SamplerDefinition& def);

	SPT_NODISCARD static lib::SharedRef<DescriptorHeap> CreateDescriptorHeap(const RendererResourceName& name, const rhi::DescriptorHeapDefinition& definition);

	template<typename TDSState>
	SPT_NODISCARD static lib::MTHandle<TDSState>                     CreateDescriptorSetState(const RendererResourceName& name, const DescriptorSetStateParams& params = DescriptorSetStateParams());
	SPT_NODISCARD static lib::SharedRef<DescriptorSetLayout>         CreateDescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def);

	SPT_NODISCARD static lib::SharedRef<QueryPool> CreateQueryPool(const rhi::QueryPoolDefinition& def);

private:

	// block creating instance
	ResourcesManager() = default;
};


template<typename TDSState>
lib::MTHandle<TDSState> ResourcesManager::CreateDescriptorSetState(const RendererResourceName& name, const DescriptorSetStateParams& params /*= DescriptorSetStateParams()*/)
{
	return lib::MTHandle<TDSState>(new TDSState(name, params));
}

} // spt::rdr