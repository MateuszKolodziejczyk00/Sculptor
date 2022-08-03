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
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	lib::TickingTimer timer;

	while (!m_window->ShouldClose())
	{
		SPT_PROFILE_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();

		renderer::Renderer::BeginFrame();

		m_window->BeginFrame();

		rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
		const lib::SharedPtr<renderer::Semaphore> acquireSemaphore = renderer::RendererBuilder::CreateSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

		const lib::SharedPtr<renderer::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

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

		renderer::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);

		m_window->PresentTexture({ finishCommandsSemaphore });

		m_window->Update(deltaTime);

		renderer::Renderer::EndFrame();
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	m_window.reset();

	renderer::Renderer::Shutdown();
}

}
