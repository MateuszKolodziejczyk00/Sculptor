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


namespace spt::rdr
{

lib::SharedRef<RenderContext> ResourcesManager::CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	return lib::MakeShared<RenderContext>(name, contextDef);
}

lib::SharedRef<Window> ResourcesManager::CreateWindow(lib::StringView name, const rhi::RHIWindowInitializationInfo& windowInfo)
{
	return lib::MakeShared<Window>(name, windowInfo);
}

lib::SharedRef<Event> ResourcesManager::CreateEvent(const RendererResourceName& name, const rhi::EventDefinition& definition)
{
	return lib::MakeShared<Event>(name, definition);
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<Buffer>(name, definition, allocationInfo);
}

lib::SharedRef<rdr::Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, const rhi::RHIBuffer& bufferInstance)
{
	return lib::MakeShared<Buffer>(name, bufferInstance);
}

lib::SharedRef<Texture> ResourcesManager::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<Texture>(name, textureDefinition, allocationInfo);
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

DescriptorSetWriter ResourcesManager::CreateDescriptorSetWriter()
{
	return DescriptorSetWriter();
}

lib::SharedRef<QueryPool> ResourcesManager::CreateQueryPool(const rhi::QueryPoolDefinition& def)
{
	return lib::MakeShared<QueryPool>(def);
}

} // spt::rdr
