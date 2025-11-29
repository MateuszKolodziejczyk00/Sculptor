#include "GPUDeferredUploadsQueue.h"
#include "EngineFrame.h"
#include "JobSystem.h"
#include "ResourcesManager.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Texture.h"
#include "Renderer.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "Transfers/UploadUtils.h"
#include "GPUDeferredUploadsQueueTypes.h"


namespace spt::gfx
{

SPT_DEFINE_PLUGIN(GPUDeferredUploadsQueue);


void GPUDeferredUploadsQueue::RequestUpload(lib::UniquePtr<GPUDeferredUploadRequest> request)
{
	m_requests.emplace_back(std::move(request));
}

void GPUDeferredUploadsQueue::ForceFlushUploads()
{
	SPT_PROFILER_FUNCTION();

	ExecuteUploads();
}

void GPUDeferredUploadsQueue::OnPostEngineInit()
{
	SPT_PROFILER_FUNCTION();

	Super::OnPostEngineInit();

	// Release References to textures pending uploads
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]()
															{
																SPT_PROFILER_FUNCTION();
																m_requests.clear();
															});
}


void GPUDeferredUploadsQueue::OnBeginFrame(engn::FrameContext& frameContext)
{
	SPT_PROFILER_FUNCTION();

	Super::OnBeginFrame(frameContext);

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [this]
			   {
				   ExecuteUploads();
			   },
			   js::Prerequisites(frameContext.GetStageBeginEvent(engn::EFrameStage::TransferDataForRendering)),
			   js::JobDef().ExecuteBefore(frameContext.GetStageBeginEvent(engn::EFrameStage::RenderingBegin)));
}

void GPUDeferredUploadsQueue::ExecuteUploads()
{
	SPT_PROFILER_FUNCTION();

	if (!m_requests.empty())
	{
		const lib::SharedPtr<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("TextureStreamer"), rhi::ContextDefinition());

		ExecutePreUploadBarriers(renderContext);

		for (const lib::UniquePtr<GPUDeferredUploadRequest>& request : m_requests)
		{
			request->EnqueueUploads();
		}

		gfx::FlushPendingUploads();

		ExecutePostUploadBarriers(renderContext);

		m_requests.clear();
	}

	m_postDeferredUploadsDelegate.Broadcast();
}

void GPUDeferredUploadsQueue::ExecutePreUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext)
{
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Pre Upload Barriers Cmd Buffer"),
																								 lib::Ref(renderContext),
																								 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));
	recorder->BeginDebugRegion("Pre Upload Barriers", lib::Color::Green);

	rhi::RHIDependency dependency;

	for (const lib::UniquePtr<GPUDeferredUploadRequest>& request : m_requests)
	{
		request->PrepareForUpload(*recorder, dependency);
	}

	recorder->ExecuteBarrier(dependency);

	recorder->EndDebugRegion();

	const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

	rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::CorePipe, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers));
}

void GPUDeferredUploadsQueue::ExecutePostUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext)
{
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Post Upload Barriers Cmd Buffer"),
																								 lib::Ref(renderContext),
																								 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));

	recorder->BeginDebugRegion("Post Upload Barriers", lib::Color::Green);

	rhi::RHIDependency dependency;

	for (const lib::UniquePtr<GPUDeferredUploadRequest>& request : m_requests)
	{
		request->FinishStreaming(*recorder, dependency);
	}

	recorder->ExecuteBarrier(dependency);

	recorder->EndDebugRegion();

	const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

	rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::CorePipe, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers));
}

} // spt::gfx
