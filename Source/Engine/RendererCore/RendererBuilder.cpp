#include "RendererBuilder.h"
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

lib::SharedRef<Context> RendererBuilder::CreateContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	return lib::MakeShared<Context>(name, contextDef);
}

lib::SharedRef<Window> RendererBuilder::CreateWindow(lib::StringView name, math::Vector2u resolution)
{
	return lib::MakeShared<Window>(name, resolution);
}

lib::SharedRef<UIBackend> RendererBuilder::CreateUIBackend(ui::UIContext context, const lib::SharedRef<Window>& window)
{
	return lib::MakeShared<UIBackend>(context, window);
}

lib::SharedRef<Buffer> RendererBuilder::CreateBuffer(const RendererResourceName& name, Uint64 size, rhi::EBufferUsage bufferUsage, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<Buffer>(name, size, bufferUsage, allocationInfo);
}

lib::SharedRef<Texture> RendererBuilder::CreateTexture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo)
{
	return lib::MakeShared<Texture>(name, textureDefinition, allocationInfo);
}

lib::SharedRef<Semaphore> RendererBuilder::CreateSemaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	return lib::MakeShared<Semaphore>(name, definition);
}

lib::SharedRef<CommandBuffer> RendererBuilder::CreateCommandBuffer(const RendererResourceName& name, const rhi::CommandBufferDefinition& definition)
{
	return lib::MakeShared<CommandBuffer>(name, definition);
}

Barrier RendererBuilder::CreateBarrier()
{
	return Barrier();
}

lib::SharedRef<Shader> RendererBuilder::CreateShader(const RendererResourceName& name, const lib::DynamicArray<rhi::ShaderModuleDefinition>& moduleDefinitions, const lib::SharedRef<smd::ShaderMetaData>& metaData)
{
	return lib::MakeShared<Shader>(name, moduleDefinitions, metaData);
}

lib::SharedRef<GraphicsPipeline> RendererBuilder::CreateGraphicsPipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader, const rhi::GraphicsPipelineDefinition pipelineDef)
{
	return lib::MakeShared<GraphicsPipeline>(name, shader, pipelineDef);
}

lib::SharedRef<ComputePipeline> RendererBuilder::CreateComputePipeline(const RendererResourceName& name, const lib::SharedRef<Shader>& shader)
{
	return lib::MakeShared<ComputePipeline>(name, shader);
}

DescriptorSetWriter RendererBuilder::CreateDescriptorSetWriter()
{
	return DescriptorSetWriter();
}

} // spt::rdr
