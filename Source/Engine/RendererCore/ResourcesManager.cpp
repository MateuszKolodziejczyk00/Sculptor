#include "ResourcesManager.h"
#include "Renderer.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesLibrary.h"
#include "Samplers/SamplersCache.h"
#include "Types/Context.h"
#include "Types/Buffer.h"
#include "Types/Texture.h"
#include "Types/Window.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/UIBackend.h"
#include "Types/Shader.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"


namespace spt::rdr
{

lib::SharedRef<Context> ResourcesManager::CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	return lib::MakeShared<Context>(name, contextDef);
}

lib::SharedRef<Window> ResourcesManager::CreateWindow(lib::StringView name, math::Vector2u resolution)
{
	return lib::MakeShared<Window>(name, resolution);
}

lib::SharedRef<UIBackend> ResourcesManager::CreateUIBackend(ui::UIContext context, const lib::SharedRef<Window>& window)
{
	return lib::MakeShared<UIBackend>(context, window);
}

lib::SharedRef<Buffer> ResourcesManager::CreateBuffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<Buffer>(name, size, bufferUsage, allocationInfo);
}

lib::SharedRef<Texture> ResourcesManager::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<Texture>(name, textureDefinition, allocationInfo);
}

lib::SharedRef<Semaphore> ResourcesManager::CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	return lib::MakeShared<Semaphore>(name, definition);
}

lib::SharedRef<CommandBuffer> ResourcesManager::CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition)
{
	return lib::MakeShared<CommandBuffer>(name, definition);
}

Barrier ResourcesManager::CreateBarrier()
{
	return Barrier();
}

ShaderID ResourcesManager::CreateShader(const lib::String& shaderRelativePath, const sc::ShaderCompilationSettings& settings, EShaderFlags flags /*= EShaderFlags::None*/)
{
	return Renderer::GetShadersManager().CreateShader(shaderRelativePath, settings, flags);
}

lib::SharedRef<Shader> ResourcesManager::GetShaderObject(ShaderID shaderID)
{
	return Renderer::GetShadersManager().GetShader(shaderID);
}

DescriptorSetWriter ResourcesManager::CreateDescriptorSetWriter()
{
	return DescriptorSetWriter();
}

PipelineStateID ResourcesManager::CreateGfxPipeline(const RendererResourceName& nameInNotCached, const rhi::GraphicsPipelineDefinition& pipelineDef, const ShaderID& shader)
{
	return Renderer::GetPipelinesLibrary().GetOrCreateGfxPipeline(nameInNotCached, pipelineDef, shader);
}

PipelineStateID ResourcesManager::CreateComputePipeline(const RendererResourceName& nameInNotCached, const ShaderID& shader)
{
	return Renderer::GetPipelinesLibrary().GetOrCreateComputePipeline(nameInNotCached, shader);
}

lib::SharedRef<GraphicsPipeline> ResourcesManager::GetGraphicsPipeline(PipelineStateID id)
{
	return Renderer::GetPipelinesLibrary().GetGraphicsPipeline(id);
}

lib::SharedRef<ComputePipeline> ResourcesManager::GetComputePipeline(PipelineStateID id)
{
	return Renderer::GetPipelinesLibrary().GetComputePipeline(id);
}

lib::SharedRef<Sampler> ResourcesManager::CreateSampler(const rhi::SamplerDefinition& def)
{
	return Renderer::GetSamplersCache().GetOrCreateSampler(def);
}

} // spt::rdr
