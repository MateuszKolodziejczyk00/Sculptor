#include "GPUDeferredCommandsQueue.h"
#include "EngineFrame.h"
#include "JobSystem.h"
#include "ResourcesManager.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/AccelerationStructure.h"
#include "Types/Texture.h"
#include "Renderer.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "Transfers/UploadUtils.h"
#include "GPUDeferredCommandsQueueTypes.h"


namespace spt::gfx
{

SPT_DEFINE_PLUGIN(GPUDeferredCommandsQueue);


void GPUDeferredCommandsQueue::RequestUpload(lib::UniquePtr<GPUDeferredUploadRequest> request)
{
	const lib::LockGuard lock(m_requestsLock);

	m_uploadRequests.emplace_back(std::move(request));
}

void GPUDeferredCommandsQueue::RequestBLASBuild(lib::UniquePtr<GPUDeferredBLASBuildRequest> request)
{
	const lib::LockGuard lock(m_requestsLock);

	m_blasBuildRequests.emplace_back(std::move(request));
}

void GPUDeferredCommandsQueue::ForceFlushCommands()
{
	SPT_PROFILER_FUNCTION();

	ExecuteCommands();
}

void GPUDeferredCommandsQueue::OnPostEngineInit()
{
	SPT_PROFILER_FUNCTION();

	Super::OnPostEngineInit();

	// Release References to textures pending uploads
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]()
															{
																SPT_PROFILER_FUNCTION();
																m_uploadRequests.clear();
															});
}


void GPUDeferredCommandsQueue::OnBeginFrame(engn::FrameContext& frameContext)
{
	SPT_PROFILER_FUNCTION();

	Super::OnBeginFrame(frameContext);

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [this]
			   {
				   ExecuteCommands();
			   },
			   js::Prerequisites(frameContext.GetStageBeginEvent(engn::EFrameStage::TransferDataForRendering)),
			   js::JobDef().ExecuteBefore(frameContext.GetStageBeginEvent(engn::EFrameStage::RenderingBegin)));
}

void GPUDeferredCommandsQueue::ExecuteCommands()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lock(m_requestsLock);

	if (!m_uploadRequests.empty() || !m_blasBuildRequests.empty())
	{
		const lib::SharedPtr<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("TextureStreamer"), rhi::ContextDefinition());

		if (!m_uploadRequests.empty())
		{
			ExecutePreUploadBarriers(renderContext);

			for (const lib::UniquePtr<GPUDeferredUploadRequest>& request : m_uploadRequests)
			{
				request->EnqueueUploads();
			}

			gfx::FlushPendingUploads();

			ExecutePostUploadBarriers(renderContext);

			m_uploadRequests.clear();
		}

		if (!m_blasBuildRequests.empty())
		{
			rdr::BLASBuilder blasBuilder;

			for (const lib::UniquePtr<GPUDeferredBLASBuildRequest>& request : m_blasBuildRequests)
			{
				request->BuildBLASes(blasBuilder);
			}

			lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("BLAS Builds Cmd Buffer"),
																										 lib::Ref(renderContext),
																										 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));

			blasBuilder.Build(*recorder);

			const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

			rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, rdr::EGPUWorkloadSubmitFlags::CorePipe);

			m_blasBuildRequests.clear();
		}
	}

	m_postDeferredUploadsDelegate.ResetAndBroadcast();
}

void GPUDeferredCommandsQueue::ExecutePreUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext)
{
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Pre Upload Barriers Cmd Buffer"),
																								 lib::Ref(renderContext),
																								 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));
	recorder->BeginDebugRegion("Pre Upload Barriers", lib::Color::Green);

	rhi::RHIDependency dependency;

	for (const lib::UniquePtr<GPUDeferredUploadRequest>& request : m_uploadRequests)
	{
		request->PrepareForUpload(*recorder, dependency);
	}

	recorder->ExecuteBarrier(dependency);

	recorder->EndDebugRegion();

	const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

	rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::CorePipe, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers));
}

void GPUDeferredCommandsQueue::ExecutePostUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext)
{
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Post Upload Barriers Cmd Buffer"),
																								 lib::Ref(renderContext),
																								 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));

	recorder->BeginDebugRegion("Post Upload Barriers", lib::Color::Green);

	rhi::RHIDependency dependency;

	for (const lib::UniquePtr<GPUDeferredUploadRequest>& request : m_uploadRequests)
	{
		request->FinishStreaming(*recorder, dependency);
	}

	recorder->ExecuteBarrier(dependency);

	recorder->EndDebugRegion();

	const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

	rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::CorePipe, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers));
}

} // spt::gfx
