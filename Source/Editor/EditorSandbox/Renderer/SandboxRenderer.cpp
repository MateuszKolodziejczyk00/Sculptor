#include "Renderer/SandboxRenderer.h"
#include "Common/ShaderCompilationInput.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
#include "Types/Window.h"
#include "Types/RenderContext.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Renderer.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "Types/Sampler.h"
#include "GPUDiagnose/Profiler/GPUProfiler.h"
#include "GPUDiagnose/Debug/GPUDebug.h"
#include "JobSystem.h"
#include "RenderGraphBuilder.h"
#include "Engine.h"
#include "Loaders/glTFSceneLoader.h"
#include "Paths.h"
#include "BufferUtilities.h"

namespace spt::ed
{

SandboxRenderer::SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow)
	: m_window(std::move(owningWindow))
{
	const rdr::ShaderID shaderID = rdr::ResourcesManager::CreateShader("Test.hlsl", sc::ShaderCompilationSettings());
	m_computePipelineID = rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME("CompTest"), shaderID);

	rhi::TextureDefinition textureDef;
	textureDef.resolution = math::Vector3u(1920, 1080, 1);
	textureDef.usage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);

	rhi::TextureViewDefinition viewDef;
	viewDef.subresourceRange.aspect = rhi::ETextureAspect::Color;

	m_descriptorSet = rdr::ResourcesManager::CreateDescriptorSetState<TestDS>(RENDERER_RESOURCE_NAME("TestDS"), rdr::EDescriptorSetStateFlags::Persistent);

	m_texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("TestTexture"), textureDef, rhi::RHIAllocationInfo());
	const lib::SharedRef<rdr::TextureView> textureView = m_texture->CreateView(RENDERER_RESOURCE_NAME("TestTextureView"), viewDef);

	m_descriptorSet->u_texture.Set(textureView);

	const rhi::SamplerDefinition samplerDef(rhi::ESamplerFilterType::Linear, rhi::EMipMapAddressingMode::Nearest, rhi::EAxisAddressingMode::Repeat);
	const lib::SharedRef<rdr::Sampler> sampler = rdr::ResourcesManager::CreateSampler(samplerDef);
	m_uiTextureID = rdr::UIBackend::GetUITextureID(textureView, sampler);
	
	const lib::HashedString scenePath = engn::Engine::Get().GetCmdLineArgs().GetValue("-Scene");
	if (scenePath.IsValid())
	{
		const lib::String finalPath = engn::Paths::Combine(engn::Paths::GetContentPath(), scenePath.ToString());
		rsc::glTFLoader::LoadScene(m_renderScene, finalPath);
	}
}

void SandboxRenderer::Tick(Real32 deltaTime)
{
	m_descriptorSet->u_viewInfo.Set([](TestViewInfo& info)
								   {
									   info.color = math::Vector4f::Constant(sin(engn::Engine::Get().GetTime()));
								   });
}

lib::SharedPtr<rdr::Semaphore> SandboxRenderer::RenderFrame()
{
	SPT_PROFILER_FUNCTION();

	js::JobWithResult createFinishSemaphoreJob = js::Launch(SPT_GENERIC_JOB_NAME, []() -> lib::SharedRef<rdr::Semaphore>
															{
																const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
																return rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);
															});

	js::JobWithResult flushPendingBufferUploads = js::Launch(SPT_GENERIC_JOB_NAME, []() -> lib::SharedPtr<rdr::Semaphore>
															 {
																 return gfx::FlushPendingUploads();
															 });

	rg::RenderGraphBuilder graphBuilder;

	const rg::RGTextureHandle texture = graphBuilder.AcquireExternalTexture(m_texture);

	graphBuilder.AddDispatch(RG_DEBUG_NAME("Sandbox Dispatch"),
							 m_computePipelineID,
							 math::Vector3u(240, 135, 1),
							 rg::BindDescriptorSets(lib::Ref(m_descriptorSet)));

	// prepare for UI pass
	graphBuilder.ReleaseTextureWithTransition(texture, rhi::TextureTransition::FragmentReadOnly);

	rdr::SemaphoresArray waitSemaphores;
	// Wait for previous frame as we're reusing resources
	waitSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx() - 1, rhi::EPipelineStage::ALL_COMMANDS);
	
	const lib::SharedPtr<rdr::Semaphore> finishPendingUploadsSemaphore = flushPendingBufferUploads.Await();
	if (finishPendingUploadsSemaphore)
	{
		waitSemaphores.AddBinarySemaphore(finishPendingUploadsSemaphore, rhi::EPipelineStage::ALL_COMMANDS);
	}

	const lib::SharedRef<rdr::Semaphore> finishSemaphore = createFinishSemaphoreJob.Await();
	rdr::SemaphoresArray signalSemaphores;
	signalSemaphores.AddBinarySemaphore(finishSemaphore, rhi::EPipelineStage::ALL_COMMANDS);

	graphBuilder.Execute(waitSemaphores, signalSemaphores);

	return finishSemaphore.ToSharedPtr();
}

ui::TextureID SandboxRenderer::GetUITextureID() const
{
	return m_uiTextureID;
}

ed::TestDS& SandboxRenderer::GetDescriptorSet()
{
	return *m_descriptorSet;
}

const lib::SharedPtr<rdr::Window>& SandboxRenderer::GetWindow() const
{
	return m_window;
}

} // spt::ed
