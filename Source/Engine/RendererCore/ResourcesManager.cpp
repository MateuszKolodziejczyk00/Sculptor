#include "ResourcesManager.h"
#include "GPUApi.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesCache.h"
#include "Samplers/SamplersCache.h"
#include "Types/RenderContext.h"
#include "Types/Buffer.h"
#include "Types/Sampler.h"
#include "Types/Texture.h"
#include "Types/Window.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/UIBackend.h"
#include "Types/Shader.h"
#include "Types/AccelerationStructure.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/GPUEvent.h"
#include "Types/QueryPool.h"
#include "Types/GPUMemoryPool.h"
#include "Types/DescriptorSetLayout.h"
#include "Types/DescriptorHeap.h"
#include "CommandsRecorder/CommandRecorder.h"


namespace spt::rdr
{

namespace utils
{

static lib::SharedRef<RenderContext> CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef /*= rhi::ContextDefinition(*/)
{
	return lib::MakeShared<RenderContext>(name, contextDef);
}

static lib::UniquePtr<rdr::CommandRecorder> CreateCommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage /*= rhi::CommandBufferUsageDefinition()*/)
{
	return std::make_unique<rdr::CommandRecorder>(name, context, cmdBufferDef, commandBufferUsage);
}

static lib::SharedRef<Window> CreateRenderWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo)
{
	return lib::MakeShared<Window>(name, windowInfo);
}

static lib::SharedRef<GPUEvent> CreateGPUEvent(const RendererResourceName& name, const rhi::EventDefinition& definition)
{
	return lib::MakeShared<GPUEvent>(name, definition);
}

static lib::SharedRef<Buffer> CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition, BufferViewDescriptorsAllocation descriptorsAllocation /*= BufferViewDescriptorsAllocation{}*/)
{
	return lib::MakeShared<Buffer>(name, definition, allocationDefinition, std::move(descriptorsAllocation));
}

static lib::SharedRef<Buffer> CreateBufferFromInstance(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance)
{
	return lib::MakeShared<Buffer>(name, bufferInstance);
}

static lib::SharedRef<Texture> CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
	return lib::MakeShared<Texture>(name, textureDefinition, allocationDefinition);
}

static lib::SharedRef<GPUMemoryPool> CreateGPUMemoryPool(const RendererResourceName& name, const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<GPUMemoryPool>(name, definition, allocationInfo);
}

static lib::SharedRef<Semaphore> CreateRenderSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	return lib::MakeShared<Semaphore>(name, definition);
}

static lib::SharedRef<CommandBuffer> CreateCommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition)
{
	return lib::MakeShared<CommandBuffer>(name, renderContext, definition);
}

static lib::SharedRef<BottomLevelAS> CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition)
{
	return lib::MakeShared<BottomLevelAS>(name, definition);
}

static lib::SharedRef<TopLevelAS> CreateTLAS(const RendererResourceName& name, const rhi::TLASDefinition& definition)
{
	return lib::MakeShared<TopLevelAS>(name, definition);
}

static lib::SharedRef<DescriptorHeap> CreateDescriptorHeap(const RendererResourceName& name, const rhi::DescriptorHeapDefinition& definition)
{
	return lib::MakeShared<DescriptorHeap>(name, definition);
}

static lib::SharedRef<DescriptorSetLayout> CreateDescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def)
{
	return lib::MakeShared<DescriptorSetLayout>(name, def);
}

static lib::SharedRef<QueryPool> CreateQueryPool(const rhi::QueryPoolDefinition& def)
{
	return lib::MakeShared<QueryPool>(def);
}

static lib::SharedRef<Sampler> CreateSampler(const rhi::SamplerDefinition& def)
{
	return lib::MakeShared<Sampler>(def);
}

static lib::SharedRef<GraphicsPipeline> CreateGfxPipeline(const RendererResourceName& name, const GraphicsPipelineShadersDefinition& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return lib::MakeShared<GraphicsPipeline>(name, shaders, pipelineDef);
}

static lib::SharedRef<ComputePipeline> CreateComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader)
{
	return lib::MakeShared<ComputePipeline>(name, shader);
}

static lib::SharedRef<RayTracingPipeline> CreateRayTracingPipeline(const RendererResourceName& name, const RayTracingPipelineShaderObjects& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return lib::MakeShared<RayTracingPipeline>(name, shaders, pipelineDef);
}

static lib::SharedRef<Shader> CreateShader(const RendererResourceName& name, const rhi::ShaderModuleDefinition& moduleDef, spt::smd::ShaderMetaData&& metaData)
{
	return lib::MakeShared<Shader>(name, moduleDef, std::move(metaData));
}

} // utils

struct GPUApiFactory
{
	lib::RawCallable<lib::SharedRef<RenderContext>(const RendererResourceName& /* name */, const rhi::ContextDefinition& /* contextDef */)> createContextFunc;
	lib::RawCallable<lib::UniquePtr<rdr::CommandRecorder>(const rdr::RendererResourceName& /* name */, const lib::SharedRef<RenderContext>& /* context */, const rhi::CommandBufferDefinition& /* cmdBufferDef */, const rhi::CommandBufferUsageDefinition& /* commandBufferUsage */)> createCommandRecorderFunc;
	lib::RawCallable<lib::SharedRef<Window>(lib::StringView /* name */, const rhi::RHIWindowInitializationInfo& /* windowInfo */)> createRenderWindowFunc;
	lib::RawCallable<lib::SharedRef<GPUEvent>(const RendererResourceName& /* name */, const rhi::EventDefinition& /* definition */)> createGPUEventFunc;
	lib::RawCallable<lib::SharedRef<Buffer>(const RendererResourceName& /* name */, const rhi::BufferDefinition& /* definition */, const AllocationDefinition& /* allocationDefinition */, BufferViewDescriptorsAllocation /* descriptorsAllocation */)> createBufferFunc;
	lib::RawCallable<lib::SharedRef<Buffer>(const RendererResourceName& /* name */, const rhi::RHIBuffer& /* bufferInstance */)> createBufferFromInstanceFunc;
	lib::RawCallable<lib::SharedRef<Texture>(const RendererResourceName& /* name */, const rhi::TextureDefinition& /* textureDefinition */, const AllocationDefinition& /* allocationDefinition */)> createTextureFunc;
	lib::RawCallable<lib::SharedRef<GPUMemoryPool>(const RendererResourceName& /* name */, const rhi::RHIMemoryPoolDefinition& /* definition */, const rhi::RHIAllocationInfo& /* allocationInfo */)> createGPUMemoryPoolFunc;
	lib::RawCallable<lib::SharedRef<Semaphore>(const RendererResourceName& /* name */, const rhi::SemaphoreDefinition& /* definition */)> createRenderSemaphoreFunc;
	lib::RawCallable<lib::SharedRef<CommandBuffer>(const RendererResourceName& /* name */, const lib::SharedRef<RenderContext>& /* renderContext */, const rhi::CommandBufferDefinition& /* definition */)> createCommandBufferFunc;
	lib::RawCallable<lib::SharedRef<BottomLevelAS>(const RendererResourceName& /* name */, const rhi::BLASDefinition& /* definition */)> createBLASFunc;
	lib::RawCallable<lib::SharedRef<TopLevelAS>(const RendererResourceName& /* name */, const rhi::TLASDefinition& /* definition */)> createTLASFunc;
	lib::RawCallable<lib::SharedRef<DescriptorHeap>(const RendererResourceName& /* name */, const rhi::DescriptorHeapDefinition& /* definition */)> createDescriptorHeapFunc;
	lib::RawCallable<lib::SharedRef<DescriptorSetLayout>(const RendererResourceName& /* name */, const rhi::DescriptorSetDefinition& /* def */)> createDescriptorSetLayoutFunc;
	lib::RawCallable<lib::SharedRef<QueryPool>(const rhi::QueryPoolDefinition& /* def */)> createQueryPoolFunc;
	lib::RawCallable<lib::SharedRef<Sampler>(const rhi::SamplerDefinition& /* def */)> createSamplerFunc;
	lib::RawCallable<lib::SharedRef<GraphicsPipeline>(const RendererResourceName& /* name */, const GraphicsPipelineShadersDefinition& /* shaders */, const rhi::GraphicsPipelineDefinition& /* pipelineDef */)> createGfxPipelineFunc;
	lib::RawCallable<lib::SharedRef<ComputePipeline>(const RendererResourceName& /* name */, const lib::SharedRef<Shader>& /* shader */)> createComputePipelineFunc;
	lib::RawCallable<lib::SharedRef<RayTracingPipeline>(const RendererResourceName& /* name */, const RayTracingPipelineShaderObjects& /* shaders */, const rhi::RayTracingPipelineDefinition& /* pipelineDef */)> createRayTracingPipelineFunc;
	lib::RawCallable<lib::SharedRef<Shader>(const RendererResourceName& /* name */, const rhi::ShaderModuleDefinition& /* moduleDef */, spt::smd::ShaderMetaData&& /* metaData */)> createShaderFunc;
};

GPUApiFactory* g_GPUApiFactory = nullptr;


GPUApiFactoryData* ResourcesManager::Initialize()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!g_GPUApiFactory);

	g_GPUApiFactory = new GPUApiFactory();
	g_GPUApiFactory->createContextFunc             = utils::CreateContext;
	g_GPUApiFactory->createCommandRecorderFunc     = utils::CreateCommandRecorder;
	g_GPUApiFactory->createRenderWindowFunc        = utils::CreateRenderWindow;
	g_GPUApiFactory->createGPUEventFunc            = utils::CreateGPUEvent;
	g_GPUApiFactory->createBufferFunc              = utils::CreateBuffer;
	g_GPUApiFactory->createBufferFromInstanceFunc  = utils::CreateBufferFromInstance;
	g_GPUApiFactory->createTextureFunc             = utils::CreateTexture;
	g_GPUApiFactory->createGPUMemoryPoolFunc       = utils::CreateGPUMemoryPool;
	g_GPUApiFactory->createRenderSemaphoreFunc     = utils::CreateRenderSemaphore;
	g_GPUApiFactory->createCommandBufferFunc       = utils::CreateCommandBuffer;
	g_GPUApiFactory->createBLASFunc                = utils::CreateBLAS;
	g_GPUApiFactory->createTLASFunc                = utils::CreateTLAS;
	g_GPUApiFactory->createDescriptorHeapFunc      = utils::CreateDescriptorHeap;
	g_GPUApiFactory->createDescriptorSetLayoutFunc = utils::CreateDescriptorSetLayout;
	g_GPUApiFactory->createQueryPoolFunc           = utils::CreateQueryPool;
	g_GPUApiFactory->createSamplerFunc             = utils::CreateSampler;
	g_GPUApiFactory->createGfxPipelineFunc         = utils::CreateGfxPipeline;
	g_GPUApiFactory->createComputePipelineFunc     = utils::CreateComputePipeline;
	g_GPUApiFactory->createRayTracingPipelineFunc  = utils::CreateRayTracingPipeline;
	g_GPUApiFactory->createShaderFunc              = utils::CreateShader;

	return reinterpret_cast<GPUApiFactoryData*>(g_GPUApiFactory);
}

void ResourcesManager::InitializeModule(GPUApiFactoryData* data)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!g_GPUApiFactory);
	g_GPUApiFactory = reinterpret_cast<GPUApiFactory*>(data);
}

lib::SharedRef<RenderContext> ResourcesManager::CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef /*= rhi::ContextDefinition(*/)
{
	return g_GPUApiFactory->createContextFunc(name, contextDef);
}

lib::UniquePtr<rdr::CommandRecorder> ResourcesManager::CreateCommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage /*= rhi::CommandBufferUsageDefinition()*/)
{
	return g_GPUApiFactory->createCommandRecorderFunc(name, context, cmdBufferDef, commandBufferUsage);
}

lib::SharedRef<Window> ResourcesManager::CreateRenderWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo)
{
	return g_GPUApiFactory->createRenderWindowFunc(name, windowInfo);
}

lib::SharedRef<GPUEvent> ResourcesManager::CreateGPUEvent(const RendererResourceName& name, const rhi::EventDefinition& definition)
{
	return g_GPUApiFactory->createGPUEventFunc(name, definition);
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition, BufferViewDescriptorsAllocation descriptorsAllocation /*= BufferViewDescriptorsAllocation{}*/)
{
	rhi::BufferDefinition newDefinition = definition;

	// In some configurations we add additional flags for debugging purposes
#if SPT_DEBUG || SPT_DEVELOPMENT
	lib::AddFlag(newDefinition.usage, rhi::EBufferUsage::TransferSrc);
#endif

	return g_GPUApiFactory->createBufferFunc(name, newDefinition, allocationDefinition, std::move(descriptorsAllocation));
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance)
{
	return g_GPUApiFactory->createBufferFromInstanceFunc(name, bufferInstance);
}

lib::SharedRef<Texture> ResourcesManager::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
	rhi::TextureDefinition newDefinition = textureDefinition;

	// In some configurations we add additional flags for debugging purposes
#if SPT_DEBUG || SPT_DEVELOPMENT
	lib::AddFlag(newDefinition.usage, rhi::ETextureUsage::TransferSource);
#endif

	lib::SharedRef<Texture> texture = g_GPUApiFactory->createTextureFunc(name, newDefinition, allocationDefinition);

	const Bool autoGPUInit = !lib::HasAnyFlag(newDefinition.flags, rhi::ETextureFlags::SkipAutoGPUInit);
	SPT_CHECK(!autoGPUInit || allocationDefinition.IsCommitted());

	if (autoGPUInit)
	{
		GPUApi::GetDeviceQueuesManager().GPUInitTexture(texture);
	}

	return texture;
}

lib::SharedRef<TextureView> ResourcesManager::CreateTextureView(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
	const lib::SharedRef<Texture> texture = CreateTexture(name, textureDefinition, allocationDefinition);
	return texture->CreateView(RENDERER_RESOURCE_NAME_FORMATTED("{} View", name.Get().ToString()));
}

lib::SharedRef<GPUMemoryPool> ResourcesManager::CreateGPUMemoryPool(const RendererResourceName& name, const rhi::RHIMemoryPoolDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return g_GPUApiFactory->createGPUMemoryPoolFunc(name, definition, allocationInfo);
}

lib::SharedRef<Semaphore> ResourcesManager::CreateRenderSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	return g_GPUApiFactory->createRenderSemaphoreFunc(name, definition);
}

lib::SharedRef<CommandBuffer> ResourcesManager::CreateCommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition)
{
	return g_GPUApiFactory->createCommandBufferFunc(name, renderContext, definition);
}

ShaderID ResourcesManager::GenerateShaderID(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings /*= sc::ShaderCompilationSettings()*/, EShaderFlags flags /*= EShaderFlags::None*/)
{
	return GPUApi::GetShadersManager().GenerateShaderID(shaderRelativePath, shaderStageDef, compilationSettings);
}

ShaderID ResourcesManager::CreateShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings /*= sc::ShaderCompilationSettings()*/, EShaderFlags flags /*= EShaderFlags::None*/)
{
	return GPUApi::GetShadersManager().CreateShader(shaderRelativePath, shaderStageDef, compilationSettings, flags);
}

lib::SharedRef<Shader> ResourcesManager::GetShaderObject(ShaderID shaderID)
{
	return GPUApi::GetShadersManager().GetShader(shaderID);
}

lib::SharedRef<Shader> ResourcesManager::CreateShaderObject(const RendererResourceName& name, const rhi::ShaderModuleDefinition& moduleDef, spt::smd::ShaderMetaData&& metaData)
{
	return g_GPUApiFactory->createShaderFunc(name, moduleDef, std::move(metaData));
}

lib::SharedRef<GraphicsPipeline> ResourcesManager::CreateGfxPipelineObject(const RendererResourceName& name, const GraphicsPipelineShadersDefinition& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return g_GPUApiFactory->createGfxPipelineFunc(name, shaders, pipelineDef);
}

lib::SharedRef<ComputePipeline> ResourcesManager::CreateComputePipelineObject(const RendererResourceName& name, const lib::SharedRef<Shader>& shader)
{
	return g_GPUApiFactory->createComputePipelineFunc(name, shader);
}

lib::SharedRef<RayTracingPipeline> ResourcesManager::CreateRayTracingPipelineObject(const RendererResourceName& name, const RayTracingPipelineShaderObjects& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return g_GPUApiFactory->createRayTracingPipelineFunc(name, shaders, pipelineDef);
}

lib::SharedPtr<Pipeline> ResourcesManager::GetPipelineObject(PipelineStateID id)
{
	return GPUApi::GetPipelinesCache().GetPipeline(id);
}

lib::SharedRef<BottomLevelAS> ResourcesManager::CreateBLAS(const RendererResourceName& name, const rhi::BLASDefinition& definition)
{
	return lib::MakeShared<BottomLevelAS>(name, definition);
}

lib::SharedRef<TopLevelAS> ResourcesManager::CreateTLAS(const RendererResourceName& name, const rhi::TLASDefinition& definition)
{
	return lib::MakeShared<TopLevelAS>(name, definition);
}

PipelineStateID ResourcesManager::CreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	return GPUApi::GetPipelinesCache().GetOrCreateComputePipeline(nameInNotCached, shader);
}

PipelineStateID ResourcesManager::GeneratePipelineID(const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return GPUApi::GetPipelinesCache().GeneratePipelineID(shaders, pipelineDef);
}

PipelineStateID ResourcesManager::GeneratePipelineID(const ShaderID& shader)
{
	return GPUApi::GetPipelinesCache().GeneratePipelineID(shader);
}

PipelineStateID ResourcesManager::GeneratePipelineID(const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return GPUApi::GetPipelinesCache().GeneratePipelineID(shaders, pipelineDef);
}

PipelineStateID ResourcesManager::CreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return GPUApi::GetPipelinesCache().GetOrCreateGfxPipeline(nameInNotCached, shaders, pipelineDef);
}

PipelineStateID ResourcesManager::CreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return GPUApi::GetPipelinesCache().GetOrCreateRayTracingPipeline(nameInNotCached, shaders, pipelineDef);
}

lib::SharedRef<Sampler> ResourcesManager::CreateSampler(const rhi::SamplerDefinition& def)
{
	return GPUApi::GetSamplersCache().GetOrCreateSampler(def);
}

lib::SharedRef<Sampler> ResourcesManager::CreateSamplerObject(const rhi::SamplerDefinition& def)
{
	return g_GPUApiFactory->createSamplerFunc(def);
}

lib::SharedRef<DescriptorHeap> ResourcesManager::CreateDescriptorHeap(const RendererResourceName& name, const rhi::DescriptorHeapDefinition& definition)
{
	return g_GPUApiFactory->createDescriptorHeapFunc(name, definition);
}

lib::SharedRef<DescriptorSetLayout> ResourcesManager::CreateDescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def)
{
	return g_GPUApiFactory->createDescriptorSetLayoutFunc(name, def);
}

lib::SharedRef<QueryPool> ResourcesManager::CreateQueryPool(const rhi::QueryPoolDefinition& def)
{
	return g_GPUApiFactory->createQueryPoolFunc(def);
}

} // spt::rdr
