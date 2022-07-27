#include "Renderer.h"
#include "RendererUtils.h"
#include "CommandsRecorder/CommandsRecorder.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "CurrentFrameContext.h"
#include "Window/PlatformWindowImpl.h"
#include "RHIInitialization.h"
#include "RHIImpl.h"
#include "RHISemaphoreImpl.h"
#include "RHICommandBufferImpl.h"
#include "RHISubmitTypes.h"


namespace spt::renderer
{

void Renderer::Initialize()
{
	platform::PlatformWindow::Initialize();

	CurrentFrameContext::Initialize(RendererUtils::GetFramesInFlightNum());

	const platform::RequiredExtensionsInfo requiredExtensions = platform::PlatformWindow::GetRequiredRHIExtensionNames();

	rhi::RHIInitializationInfo initializationInfo;
	initializationInfo.m_extensions = requiredExtensions.m_extensions;
	initializationInfo.m_extensionsNum = requiredExtensions.m_extensionsNum;

	rhi::RHI::Initialize(initializationInfo);
}

void Renderer::Shutdown()
{
	CurrentFrameContext::Shutdown();

	rhi::RHI::Uninitialize();
}

void Renderer::BeginFrame()
{
	SPT_PROFILE_FUNCTION();

	CurrentFrameContext::BeginFrame();
}

void Renderer::EndFrame()
{
	SPT_PROFILE_FUNCTION();

	CurrentFrameContext::EndFrame();
}

lib::UniquePtr<CommandsRecorder> Renderer::StartRecordingCommands(const CommandsRecordingInfo& recordingInfo)
{
	SPT_PROFILE_FUNCTION();

	return std::make_unique<CommandsRecorder>(recordingInfo);
}

void Renderer::SubmitCommands(const lib::DynamicArray<CommandsSubmitBatch>& submitBatches)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(!submitBatches.empty());

	lib::DynamicArray<rhi::SubmitBatchData> rhiSubmitBatches(submitBatches.size());

	for (SizeType idx = 0; idx < submitBatches.size(); ++idx)
	{
		const CommandsSubmitBatch& submitBatch = submitBatches[idx];

		rhi::SubmitBatchData& rhiSubmitBatch = rhiSubmitBatches[idx];

		rhiSubmitBatch.m_waitSemaphores = &submitBatch.m_waitSemaphores.GetRHISemaphores();
		rhiSubmitBatch.m_signalSemaphores = &submitBatch.m_signalSemaphores.GetRHISemaphores();
		std::transform(	submitBatch.m_recordedCommands.cbegin(), submitBatch.m_recordedCommands.cend(),
						std::back_inserter(rhiSubmitBatch.m_commandBuffers),
						[](const lib::UniquePtr<CommandsRecorder>& recorder) -> const rhi::RHICommandBuffer*
						{
							return &recorder->GetCommandsBuffer()->GetRHI();
						});
	}

	rhi::RHI::SubmitCommands(rhiSubmitBatches);
}

}
