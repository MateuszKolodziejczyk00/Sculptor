#include "SculptorEdApplication.h"
#include "Renderer.h"
#include "RendererBuilder.h"
#include "Profiler.h"
#include "Timer/TickingTimer.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"
#include "CommandsRecorder/CommandsRecorder.h"


namespace spt::ed
{

SculptorEdApplication::SculptorEdApplication()
{
}

void SculptorEdApplication::OnInit()
{
	Super::OnInit();

	renderer::Renderer::Initialize();

	m_window = renderer::RendererBuilder::CreateWindow("SculptorEd", math::Vector2u(1920, 1080));

	renderer::Renderer::PostCreatedWindow();
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	m_window->InitializeUI();
	lib::SharedPtr<renderer::UIBackend> uiBackend = renderer::RendererBuilder::CreateUIBackend(m_window->GetUIContext(), m_window);;

	lib::TickingTimer timer;

	while (true)
	{
		SPT_PROFILE_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();

		m_window->Update(deltaTime);

		if (m_window->ShouldClose())
		{
			break;
		}

		renderer::Renderer::BeginFrame();

		m_window->BeginFrame();

		RenderFrame();

		renderer::Renderer::EndFrame();
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	renderer::Renderer::WaitIdle();

	m_window->UninitializeUI();

	m_window.reset();

	renderer::Renderer::Shutdown();
}

void SculptorEdApplication::RenderFrame()
{
	const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
	const lib::SharedPtr<renderer::Semaphore> acquireSemaphore = renderer::RendererBuilder::CreateSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

	const lib::SharedPtr<renderer::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

	if (m_window->IsSwapchainOutOfDate())
	{
		renderer::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
		renderer::Renderer::IncrementReleaseSemaphoreToCurrentFrame();
		return;
	}

	renderer::CommandsRecordingInfo recordingInfo;
	recordingInfo.m_commandsBufferName = RENDERER_RESOURCE_NAME("TransferCmdBuffer");
	recordingInfo.m_commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	lib::UniquePtr<renderer::CommandsRecorder> recorder = renderer::Renderer::StartRecordingCommands(recordingInfo);

	recorder->StartRecording(rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	renderer::Barrier barrier = renderer::RendererBuilder::CreateBarrier();
	const SizeType barrierIdx = barrier.GetRHI().AddTextureBarrier(swapchainTexture->GetRHI(), rhi::TextureSubresourceRange(rhi::ETextureAspect::Color));
	barrier.GetRHI().SetLayoutTransition(barrierIdx, rhi::TextureTransition::PresentSource);

	recorder->ExecuteBarrier(barrier);

	recorder->FinishRecording();

	lib::SharedPtr<renderer::Semaphore> finishCommandsSemaphore = renderer::RendererBuilder::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);

	lib::DynamicArray<renderer::CommandsSubmitBatch> submitBatches;
	renderer::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(renderer::CommandsSubmitBatch());
	submitBatch.m_recordedCommands.emplace_back(std::move(recorder));
	submitBatch.m_waitSemaphores.AddBinarySemaphore(acquireSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.m_signalSemaphores.AddBinarySemaphore(finishCommandsSemaphore, rhi::EPipelineStage::TopOfPipe);
	submitBatch.m_signalSemaphores.AddTimelineSemaphore(renderer::Renderer::GetReleaseFrameSemaphore(), renderer::Renderer::GetCurrentFrameIdx(), rhi::EPipelineStage::TopOfPipe);

	renderer::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

	m_window->PresentTexture({ finishCommandsSemaphore });

	if (m_window->IsSwapchainOutOfDate())
	{
		renderer::Renderer::WaitIdle();
		m_window->RebuildSwapchain();
	}
}

}
