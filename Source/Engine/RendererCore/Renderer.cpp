#include "Renderer.h"
#include "RendererUtils.h"
#include "CommandsRecorder/CommandsRecorder.h"
#include "Types/Semaphore.h"
#include "Types/CommandBuffer.h"
#include "Types/Window.h"
#include "CurrentFrameContext.h"
#include "Shaders/ShadersManager.h"
#include "Pipelines/PipelinesLibrary.h"
#include "DescriptorSets/DescriptorSetsManager.h"
#include "Profiler/GPUProfiler.h"
#include "Window/PlatformWindowImpl.h"
#include "RHICore/RHIInitialization.h"
#include "RHICore/RHISubmitTypes.h"
#include "RHIBridge/RHIImpl.h"
#include "RHIBridge/RHISemaphoreImpl.h"
#include "RHIBridge/RHICommandBufferImpl.h"


namespace spt::rdr
{

namespace priv
{

struct RendererData
{

ShadersManager shadersManager;

PipelinesLibrary pipelinesLibrary;

DescriptorSetsManager descriptorSetsManager;

};

static RendererData g_data;

} // priv

void Renderer::Initialize()
{
	platf::PlatformWindow::Initialize();

	const platf::RequiredExtensionsInfo requiredExtensions = platf::PlatformWindow::GetRequiredRHIExtensionNames();

	rhi::RHIInitializationInfo initializationInfo;
	initializationInfo.extensions		= requiredExtensions.extensions;
	initializationInfo.extensionsNum	= requiredExtensions.extensionsNum;

	rhi::RHI::Initialize(initializationInfo);

	GetShadersManager().Initialize();

	GetDescriptorSetsManager().Initialize();
}

void Renderer::PostCreatedWindow()
{
	CurrentFrameContext::Initialize(RendererUtils::GetFramesInFlightNum());

	GPUProfiler::Initialize();
}

void Renderer::Uninitialize()
{
	GetPipelinesLibrary().ClearCachedPipelines();

	GetDescriptorSetsManager().Uninitialize();

	GetShadersManager().Uninitialize();

	CurrentFrameContext::Shutdown();

	rhi::RHI::Uninitialize();
}

void Renderer::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	rhi::RHI::BeginFrame();

	CurrentFrameContext::BeginFrame();
}

void Renderer::EndFrame()
{
	SPT_PROFILER_FUNCTION();

	CurrentFrameContext::EndFrame();

	rhi::RHI::EndFrame();
}

ShadersManager& Renderer::GetShadersManager()
{
	return priv::g_data.shadersManager;
}

PipelinesLibrary& Renderer::GetPipelinesLibrary()
{
	return priv::g_data.pipelinesLibrary;
}

DescriptorSetsManager& Renderer::GetDescriptorSetsManager()
{
	return priv::g_data.descriptorSetsManager;
}

lib::UniquePtr<CommandsRecorder> Renderer::StartRecordingCommands()
{
	SPT_PROFILER_FUNCTION();

	return std::make_unique<CommandsRecorder>();
}

void Renderer::SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<CommandsSubmitBatch>& submitBatches)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!submitBatches.empty());

	lib::DynamicArray<rhi::SubmitBatchData> rhiSubmitBatches(submitBatches.size());

	for (SizeType idx = 0; idx < submitBatches.size(); ++idx)
	{
		const CommandsSubmitBatch& submitBatch = submitBatches[idx];

		rhi::SubmitBatchData& rhiSubmitBatch = rhiSubmitBatches[idx];

		rhiSubmitBatch.waitSemaphores = &submitBatch.waitSemaphores.GetRHISemaphores();
		rhiSubmitBatch.signalSemaphores = &submitBatch.signalSemaphores.GetRHISemaphores();
		std::transform(	submitBatch.recordedCommands.cbegin(), submitBatch.recordedCommands.cend(),
						std::back_inserter(rhiSubmitBatch.commandBuffers),
						[](const lib::UniquePtr<CommandsRecorder>& recorder) -> const rhi::RHICommandBuffer*
						{
							return &recorder->GetCommandsBuffer()->GetRHI();
						});
	}

	rhi::RHI::SubmitCommands(queueType, rhiSubmitBatches);
}

void Renderer::PresentTexture(const lib::SharedRef<Window>& window, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores)
{
	SPT_PROFILER_FUNCTION();

	GPUProfiler::FlipFrame(window);
	
	window->PresentTexture(waitSemaphores);
}

void Renderer::WaitIdle()
{
	SPT_PROFILER_FUNCTION();

	rhi::RHI::WaitIdle();

	CurrentFrameContext::ReleaseAllResources();
}

Uint64 Renderer::GetCurrentFrameIdx()
{
	return CurrentFrameContext::GetCurrentFrameIdx();
}

const lib::SharedPtr<Semaphore>& Renderer::GetReleaseFrameSemaphore()
{
	return CurrentFrameContext::GetReleaseFrameSemaphore();
}

void Renderer::IncrementReleaseSemaphoreToCurrentFrame()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<Semaphore>& releaseFrameSemaphore = GetReleaseFrameSemaphore();
	releaseFrameSemaphore->GetRHI().Signal(GetCurrentFrameIdx());
}

void Renderer::EnableValidationWarnings(Bool enable)
{
	rhi::RHI::EnableValidationWarnings(enable);
}

} // spt::rdr
