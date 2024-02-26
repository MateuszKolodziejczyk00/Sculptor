#include "ResourcesManager.h"
#include "Renderer.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesLibrary.h"
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
#include "Types/Event.h"
#include "Types/QueryPool.h"
#include "Types/GPUMemoryPool.h"
#include "Types/DescriptorSetLayout.h"
#include "Types/DescriptorSetState/DescriptorSetStackAllocator.h"
#include "CommandsRecorder/CommandRecorder.h"


namespace spt::rdr
{

lib::SharedRef<RenderContext> ResourcesManager::CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	return lib::MakeShared<RenderContext>(name, contextDef);
}

lib::UniquePtr<rdr::CommandRecorder> ResourcesManager::CreateCommandRecorder(const rdr::RendererResourceName& name, const lib::SharedRef<RenderContext>& context, const rhi::CommandBufferDefinition& cmdBufferDef, const rhi::CommandBufferUsageDefinition& commandBufferUsage /*= rhi::CommandBufferUsageDefinition()*/)
{
	return std::make_unique<rdr::CommandRecorder>(name, context, cmdBufferDef, commandBufferUsage);
}

lib::SharedRef<Window> ResourcesManager::CreateWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo)
{
	return lib::MakeShared<Window>(name, windowInfo);
}

lib::SharedRef<Event> ResourcesManager::CreateEvent(const RendererResourceName& name, const rhi::EventDefinition& definition)
{
	return lib::MakeShared<Event>(name, definition);
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition)
{
	return lib::MakeShared<Buffer>(name, definition, allocationDefinition);
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance)
{
	return lib::MakeShared<Buffer>(name, bufferInstance);
}

lib::SharedRef<Texture> ResourcesManager::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
	return lib::MakeShared<Texture>(name, textureDefinition, allocationDefinition);
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
	return Renderer::GetPipelinesLibrary().GetPipeline(id);
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
	return Renderer::GetPipelinesLibrary().GetOrCreateComputePipeline(nameInNotCached, shader);
}

PipelineStateID ResourcesManager::CreateGfxPipeline(const RendererResourceName& nameInNotCached, const GraphicsPipelineShaders& shaders, const rhi::GraphicsPipelineDefinition& pipelineDef)
{
	return Renderer::GetPipelinesLibrary().GetOrCreateGfxPipeline(nameInNotCached, shaders, pipelineDef);
}

PipelineStateID ResourcesManager::CreateRayTracingPipeline(const RendererResourceName& nameInNotCached, const RayTracingPipelineShaders& shaders, const rhi::RayTracingPipelineDefinition& pipelineDef)
{
	return Renderer::GetPipelinesLibrary().GetOrCreateRayTracingPipeline(nameInNotCached, shaders, pipelineDef);
}

lib::SharedRef<Sampler> ResourcesManager::CreateSampler(const rhi::SamplerDefinition& def)
{
	return Renderer::GetSamplersCache().GetOrCreateSampler(def);
}

lib::SharedRef<DescriptorSetLayout> ResourcesManager::CreateDescriptorSetLayout(const RendererResourceName& name, const rhi::DescriptorSetDefinition& def)
{
	return lib::MakeShared<DescriptorSetLayout>(name, def);
}

lib::SharedRef<DescriptorSetStackAllocator> ResourcesManager::CreateDescriptorSetAllocator(const RendererResourceName& name)
{
	return lib::MakeShared<DescriptorSetStackAllocator>(name);
}

lib::SharedRef<QueryPool> ResourcesManager::CreateQueryPool(const rhi::QueryPoolDefinition& def)
{
	return lib::MakeShared<QueryPool>(def);
}

} // spt::rdr
