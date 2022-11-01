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

void SandboxRenderer::RenderFrame()
{
	SPT_PROFILER_FUNCTION();

	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
	const lib::SharedRef<rdr::Semaphore> acquireSemaphore = rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

	const lib::SharedPtr<rdr::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
		rdr::Renderer::IncrementReleaseSemaphoreToCurrentFrame();
		return;
	}

	const lib::SharedRef<rdr::RenderContext> renderingContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("MainThreadContext"), rhi::ContextDefinition());

	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	{
		SPT_GPU_PROFILER_CONTEXT(recorder->GetCommandBuffer());
		SPT_GPU_DEBUG_REGION(*recorder, "FrameRegion", lib::Color::Red);

		{
			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::ColorRenderTarget);

			recorder->ExecuteBarrier(std::move(barrier));
		}

		{
			SPT_GPU_PROFILER_EVENT("TestCompute");
			SPT_GPU_DEBUG_REGION(*recorder, "TestCompute", lib::Color::Blue);

			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(m_texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::ComputeGeneral);

			recorder->ExecuteBarrier(std::move(barrier));

			recorder->BindComputePipeline(m_computePipelineID);

			recorder->BindDescriptorSetState(lib::Ref(m_descriptorSet));

			recorder->Dispatch(math::Vector3u(240, 135, 1));

			recorder->UnbindDescriptorSetState(lib::Ref(m_descriptorSet));
		}

		rhi::TextureViewDefinition viewDefinition;
		viewDefinition.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);
		const lib::SharedRef<rdr::TextureView> swapchainTextureView = swapchainTexture->CreateView(RENDERER_RESOURCE_NAME("TextureRenderView"), viewDefinition);

		{
			rdr::RenderingDefinition renderingDef(rhi::ERenderingFlags::None, math::Vector2i(0, 0), m_window->GetSwapchainSize());
			rdr::RTDefinition renderTarget;
			renderTarget.textureView = swapchainTextureView;
			renderTarget.loadOperation = rhi::ERTLoadOperation::Clear;
			renderTarget.storeOperation = rhi::ERTStoreOperation::Store;
			renderTarget.clearColor.asFloat[0] = 0.f;
			renderTarget.clearColor.asFloat[1] = 0.f;
			renderTarget.clearColor.asFloat[2] = 0.f;
			renderTarget.clearColor.asFloat[3] = 1.f;

			renderingDef.AddColorRenderTarget(renderTarget);

			SPT_GPU_PROFILER_EVENT("UI");
			SPT_GPU_DEBUG_REGION(*recorder, "UI", lib::Color::Green);

			recorder->BeginRendering(renderingDef);

			recorder->RenderUI();

			recorder->EndRendering();
		}

		{
			rdr::Barrier barrier = rdr::ResourcesManager::CreateBarrier();
			const SizeType RTBarrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(RTBarrierIdx, rhi::TextureTransition::PresentSource);
			const SizeType computeBarrierIdx = barrier.GetRHI().AddTextureBarrier(m_texture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
			barrier.GetRHI().SetLayoutTransition(computeBarrierIdx, rhi::TextureTransition::FragmentReadOnly);

			recorder->ExecuteBarrier(std::move(barrier));
		}
	}

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	recorder->RecordCommands(renderingContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::SharedRef<rdr::Semaphore> finishCommandsSemaphore = rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitBatch.recordedCommands.emplace_back(std::move(recorder));
	submitBatch.waitSemaphores.AddBinarySemaphore(acquireSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddBinarySemaphore(finishCommandsSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.signalSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx(), rhi::EPipelineStage::TopOfPipe);

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	rdr::Renderer::PresentTexture(lib::ToSharedRef(m_window), { finishCommandsSemaphore });

	if (m_window->IsSwapchainOutOfDate())
	{
		rdr::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
	}
}

ui::TextureID SandboxRenderer::GetUITextureID() const
{
	return m_uiTextureID;
}

ed::TestDS& SandboxRenderer::GetDescriptorSet()
{
	return *m_descriptorSet;
}

} // spt::ed
