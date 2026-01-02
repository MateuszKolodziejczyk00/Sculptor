#include "GPUResourceInitQueue.h"
#include "ResourcesManager.h"
#include "Types/Texture.h"
#include "CommandsRecorder/CommandRecorder.h"


namespace spt::rdr
{

GPUResourceInitQueue::GPUResourceInitQueue()
{
}

void GPUResourceInitQueue::EnqueueTextureInit(const lib::SharedPtr<Texture>& texture)
{
	const lib::LockGuard lock(m_lock);

	const SizeType depIdx = m_dependency.AddTextureDependency(texture->GetRHI(), rhi::TextureSubresourceRange());
	m_dependency.SetLayoutTransition(depIdx, rhi::TextureTransition::Undefined, rhi::TextureTransition::Generic);
}

lib::SharedPtr<GPUWorkload> GPUResourceInitQueue::RecordGPUInitializations()
{
	rhi::RHIDependency dependencyCopy;

	{
		const lib::LockGuard lock(m_lock);
		dependencyCopy = std::move(m_dependency);
		m_dependency = rhi::RHIDependency();
	}

	if (!dependencyCopy.IsEmpty())
	{
		lib::SharedRef<RenderContext> renderContext = ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("GPU Resource Init Queue Context"));
		const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		lib::UniquePtr<CommandRecorder> commandRecorder = ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("GPU Resource Init Queue Command Recorder"), renderContext, cmdBufferDef);

		commandRecorder->ExecuteBarrier(dependencyCopy);

		lib::SharedRef<GPUWorkload> gpuWorkload = commandRecorder->FinishRecording();

		return gpuWorkload;
	}
	else
	{
		return nullptr;
	}
}

} // spt::rdr
