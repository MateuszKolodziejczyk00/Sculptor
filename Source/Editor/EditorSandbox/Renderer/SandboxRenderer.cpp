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

	m_descriptorSet = rdr::ResourcesManager::CreateDescriptorSetState<TestDS>(RENDERER_RESOURCE_NAME("TestDS"), rdr::EDescriptorSetStateFlags::None);

	m_texture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("TestTexture"), textureDef, rhi::RHIAllocationInfo());
	const lib::SharedRef<rdr::TextureView> textureView = m_texture->CreateView(RENDERER_RESOURCE_NAME("TestTextureView"), viewDef);

	m_descriptorSet->u_texture.Set(textureView);

	const rhi::SamplerDefinition samplerDef(rhi::ESamplerFilterType::Linear, rhi::EMipMapAddressingMode::Nearest, rhi::EAxisAddressingMode::Repeat);
	const lib::SharedRef<rdr::Sampler> sampler = rdr::ResourcesManager::CreateSampler(samplerDef);
	m_uiTextureID = rdr::UIBackend::GetUITextureID(textureView, sampler);
}

lib::SharedPtr<rdr::Semaphore> SandboxRenderer::RenderFrame()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedRef<rdr::RenderContext> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition());

	js::JobWithResult createFinishSemaphoreJob = js::Launch(SPT_GENERIC_JOB_NAME, []() -> lib::SharedRef<rdr::Semaphore>
															{
																const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
																return rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);
															});

	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	{
		SPT_GPU_PROFILER_CONTEXT(recorder->GetCommandBuffer());
		SPT_GPU_DEBUG_REGION(*recorder, "FrameRegion", lib::Color::Red);
		
		{
			SPT_GPU_PROFILER_EVENT("TestCompute");
			SPT_GPU_DEBUG_REGION(*recorder, "TestCompute", lib::Color::Blue);

			rhi::RHIDependency dependency;
			const SizeType barrierIdx = dependency.AddTextureDependency(m_texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::ComputeGeneral);

			recorder->ExecuteBarrier(std::move(dependency));

			recorder->BindComputePipeline(m_computePipelineID);

			recorder->BindDescriptorSetState(lib::Ref(m_descriptorSet));

			recorder->Dispatch(math::Vector3u(240, 135, 1));

			recorder->UnbindDescriptorSetState(lib::Ref(m_descriptorSet));
		}
		
		{
			rhi::RHIDependency dependency;
			const SizeType computeBarrierIdx = dependency.AddTextureDependency(m_texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			dependency.SetLayoutTransition(computeBarrierIdx, rhi::TextureTransition::FragmentReadOnly);

			recorder->ExecuteBarrier(std::move(dependency));
		}
	}

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	recorder->RecordCommands(renderingContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitBatch.recordedCommands.emplace_back(std::move(recorder));
	submitBatch.signalSemaphores.AddBinarySemaphore(createFinishSemaphoreJob.Await(), rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx(), rhi::EPipelineStage::TopOfPipe);

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	return createFinishSemaphoreJob.Await();
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
