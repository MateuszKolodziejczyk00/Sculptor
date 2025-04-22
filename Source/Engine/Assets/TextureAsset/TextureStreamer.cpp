#include "TextureStreamer.h"
#include "EngineFrame.h"
#include "JobSystem.h"
#include "ResourcesManager.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Texture.h"
#include "Renderer.h"
#include "DeviceQueues/DeviceQueuesManager.h"
#include "Transfers/UploadUtils.h"


namespace spt::as
{

SPT_DEFINE_PLUGIN(TextureStreamer);


void TextureStreamer::RequestTextureUploadToGPU(StreamRequest request)
{
	m_requests.emplace_back(std::move(request));
}

void TextureStreamer::ForceFlushUploads()
{
	SPT_PROFILER_FUNCTION();

	ExecuteUploads();
}

void TextureStreamer::OnPostEngineInit()
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


void TextureStreamer::OnBeginFrame(engn::FrameContext& frameContext)
{
	SPT_PROFILER_FUNCTION();

	Super::BeginFrame(frameContext);

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [this]
			   {
				   ExecuteUploads();
			   },
			   js::Prerequisites(frameContext.GetStageBeginEvent(engn::EFrameStage::TransferDataForRendering)),
			   js::JobDef().ExecuteBefore(frameContext.GetStageBeginEvent(engn::EFrameStage::RenderingBegin)));
}

void TextureStreamer::ExecuteUploads()
{
	SPT_PROFILER_FUNCTION();

	const lib::SharedPtr<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("TextureStreamer"), rhi::ContextDefinition());

	ExecutePreUploadBarriers(renderContext);

	for (const StreamRequest& request : m_requests)
	{
		request.streamableTexture->ScheduleUploads();
	}

	gfx::FlushPendingUploads();

	ExecutePostUploadBarriers(renderContext);

	m_requests.clear();
}

void TextureStreamer::ExecutePreUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext)
{
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Pre Upload Barriers Cmd Buffer"),
																								 lib::Ref(renderContext),
																								 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));
	recorder->BeginDebugRegion("Pre Upload Barriers", lib::Color::Green);

	rhi::RHIDependency dependency;

	for (const StreamRequest& request : m_requests)
	{
		request.streamableTexture->PrepareForUpload(*recorder, dependency);
	}

	recorder->ExecuteBarrier(dependency);

	recorder->EndDebugRegion();

	const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

	rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::CorePipe, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers));
}

void TextureStreamer::ExecutePostUploadBarriers(const lib::SharedPtr<rdr::RenderContext>& renderContext)
{
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("Post Upload Barriers Cmd Buffer"),
																								 lib::Ref(renderContext),
																								 rhi::CommandBufferDefinition(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary));

	recorder->BeginDebugRegion("Post Upload Barriers", lib::Color::Green);

	rhi::RHIDependency dependency;

	for (const StreamRequest& request : m_requests)
	{
		request.streamableTexture->FinishStreaming(*recorder, dependency);
	}

	recorder->ExecuteBarrier(dependency);

	recorder->EndDebugRegion();

	const lib::SharedRef<rdr::GPUWorkload> gpuWorkload = recorder->FinishRecording();

	rdr::Renderer::GetDeviceQueuesManager().Submit(gpuWorkload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::CorePipe, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers));
}

} // spt::as
