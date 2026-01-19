#include "ResourcesManager.h"
#include "Renderer.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesCache.h"
#include "Samplers/SamplersCache.h"
#include "Types/RenderContext.h"
#include "Types/Buffer.h"
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

lib::SharedRef<RenderContext> ResourcesManager::CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef /*= rhi::ContextDefinition(*/)
{
	return lib::MakeShared<RenderContext>(name, contextDef);
}

lib::UniquePtr<rdr::CommandRecorder> ResourcesManager::CreateCommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage /*= rhi::CommandBufferUsageDefinition()*/)
{
	return std::make_unique<rdr::CommandRecorder>(name, context, cmdBufferDef, commandBufferUsage);
}

lib::SharedRef<Window> ResourcesManager::CreateRenderWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo)
{
	return lib::MakeShared<Window>(name, windowInfo);
}

lib::SharedRef<GPUEvent> ResourcesManager::CreateGPUEvent(const RendererResourceName& name, const rhi::EventDefinition& definition)
{
	return lib::MakeShared<GPUEvent>(name, definition);
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition, BufferViewDescriptorsAllocation descriptorsAllocation /*= BufferViewDescriptorsAllocation{}*/)
{
	rhi::BufferDefinition newDefinition = definition;

	// In some configurations we add additional flags for debugging purposes
#if SPT_DEBUG || SPT_DEVELOPMENT
	lib::AddFlag(newDefinition.usage, rhi::EBufferUsage::TransferSrc);
#endif

	return lib::MakeShared<Buffer>(name, newDefinition, allocationDefinition, std::move(descriptorsAllocation));
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance)
{
	return lib::MakeShared<Buffer>(name, bufferInstance);
}

lib::SharedRef<Texture> ResourcesManager::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
	rhi::TextureDefinition newDefinition = textureDefinition;

	// In some configurations we add additional flags for debugging purposes
#if SPT_DEBUG || SPT_DEVELOPMENT
	lib::AddFlag(newDefinition.usage, rhi::ETextureUsage::TransferSource);
#endif

	lib::SharedRef<Texture> texture = lib::MakeShared<Texture>(name, newDefinition, allocationDefinition);

	const Bool autoGPUInit = !lib::HasAnyFlag(newDefinition.flags, rhi::ETextureFlags::SkipAutoGPUInit);
	SPT_CHECK(!autoGPUInit || allocationDefinition.IsCommitted());

	if (autoGPUInit)
	{
		Renderer::GetDeviceQueuesManager().GPUInitTexture(texture);
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
	return lib::MakeShared<GPUMemoryPool>(name, definition, allocationInfo);
}

lib::SharedRef<Semaphore> ResourcesManager::CreateRenderSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	return lib::MakeShared<Semaphore>(name, definition);
}

lib::SharedRef<CommandBuffer> ResourcesManager::CreateCommandBuffer(const RendererResourceName& name, const lib::SharedRef<RenderContext>& renderContext, const rhi::CommandBufferDefinition& definition)
{
	return lib::MakeShared<CommandBuffer>(name, renderContext, definition);
}

ShaderID ResourcesManager::GenerateShaderID(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings /*= sc::ShaderCompilationSettings()*/, EShaderFlags flags /*= EShaderFlags::None*/)
{
	return Renderer::GetShadersManager().GenerateShaderID(shaderRelativePath, shaderStageDef, compilationSettings);
}

ShaderID ResourcesManager::CreateShader(const lib::String& shaderRelativePath, const sc::ShaderStageCompilationDef& shaderStageDef, const sc::ShaderCompilationSettings& compilationSettings /*= sc::ShaderCompilationSettings()*/, EShaderFlags flags /*= EShaderFlags::None*/)
{
	return Renderer::GetShadersManager().CreateShader(shaderRelativePath, shaderStageDef, compilationSettings, flags);
}

lib::SharedRef<Shader> ResourcesManager::GetShaderObject(ShaderID shaderID)
{
	return Renderer::GetShadersManager().GetShader(shaderID);
}

lib::SharedPtr<Pipeline> ResourcesManager::GetPipelineObject(PipelineStateID id)
{
	return Renderer::GetPipelinesCache().GetPipeline(id);
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
	return Renderer::GetPipelinesCache().GetOrCreateComputePipeline(nameInNotCached, shader);
}

PipelineStateID ResourcesManager::GeneratePipelineID(const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return Renderer::GetPipelinesCache().GeneratePipelineID(shaders, pipelineDef);
}

PipelineStateID ResourcesManager::GeneratePipelineID(const ShaderID& shader)
{
	return Renderer::GetPipelinesCache().GeneratePipelineID(shader);
}

PipelineStateID ResourcesManager::GeneratePipelineID(const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return Renderer::GetPipelinesCache().GeneratePipelineID(shaders, pipelineDef);
}

PipelineStateID ResourcesManager::CreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return Renderer::GetPipelinesCache().GetOrCreateGfxPipeline(nameInNotCached, shaders, pipelineDef);
}

PipelineStateID ResourcesManager::CreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return Renderer::GetPipelinesCache().GetOrCreateRayTracingPipeline(nameInNotCached, shaders, pipelineDef);
}

lib::SharedRef<Sampler> ResourcesManager::CreateSampler(const rhi::SamplerDefinition& def)
{
	return Renderer::GetSamplersCache().GetOrCreateSampler(def);
}

lib::SharedRef<DescriptorHeap> ResourcesManager::CreateDescriptorHeap(const RendererResourceName& name, const rhi::DescriptorHeapDefinition& definition)
{
	return lib::MakeShared<DescriptorHeap>(name, definition);
}

lib::SharedRef<DescriptorSetLayout> ResourcesManager::CreateDescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def)
{
	return lib::MakeShared<DescriptorSetLayout>(name, def);
}

lib::SharedRef<QueryPool> ResourcesManager::CreateQueryPool(const rhi::QueryPoolDefinition& def)
{
	return lib::MakeShared<QueryPool>(def);
}

} // spt::rdr
