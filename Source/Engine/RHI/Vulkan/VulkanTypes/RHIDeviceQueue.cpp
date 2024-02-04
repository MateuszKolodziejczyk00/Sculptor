#include "RHIDeviceQueue.h"
#include "RHICommandBuffer.h"
#include "RHISemaphore.h"
#include "../VulkanRHI.h"
#include "../Debug/GPUCrashTracker.h"


namespace spt::vulkan
{

RHIDeviceQueue::RHIDeviceQueue()
	: m_queueHandle(VK_NULL_HANDLE)
	, m_type(rhi::EDeviceCommandQueueType::Graphics)
{ }

void RHIDeviceQueue::InitializeRHI(rhi::EDeviceCommandQueueType type, VkQueue queueHandle)
{
	SPT_CHECK(!IsValid());
	SPT_CHECK(queueHandle != VK_NULL_HANDLE);

	m_queueHandle = queueHandle;
	m_type        = type;
}

void RHIDeviceQueue::ReleaseRHI()
{
	SPT_CHECK(IsValid());

	m_queueHandle = VK_NULL_HANDLE;
	m_type        = rhi::EDeviceCommandQueueType::Graphics;
}

Bool RHIDeviceQueue::IsValid() const
{
	return m_queueHandle != VK_NULL_HANDLE;
}

rhi::EDeviceCommandQueueType RHIDeviceQueue::GetType() const
{
	return m_type;
}

void RHIDeviceQueue::SubmitCommands(const rhi::SubmitBatchData& submitBatch)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

    const RHISemaphoresArray* waitSemaphores                     = submitBatch.waitSemaphores;
    const RHISemaphoresArray* signalSemaphores                   = submitBatch.signalSemaphores;
    const lib::DynamicArray<const RHICommandBuffer*>& cmdBuffers = submitBatch.commandBuffers;

    SPT_CHECK(!cmdBuffers.empty());

    lib::DynamicArray<VkCommandBufferSubmitInfo> cmdBuffersSubmitBatch;
    cmdBuffersSubmitBatch.resize(cmdBuffers.size());

    for (SizeType cmdBufferIdx = 0; cmdBufferIdx < cmdBuffers.size(); ++cmdBufferIdx)
    {
        SPT_CHECK(cmdBuffers[cmdBufferIdx]->GetQueueType() == m_type);

        VkCommandBufferSubmitInfo& cmdBufferSubmit  = cmdBuffersSubmitBatch[cmdBufferIdx];
        cmdBufferSubmit.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdBufferSubmit.commandBuffer               = cmdBuffers[cmdBufferIdx]->GetHandle();
        cmdBufferSubmit.deviceMask                  = 0;
    }

    VkSubmitInfo2 submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submitInfo.flags                    = 0;
    submitInfo.waitSemaphoreInfoCount   = waitSemaphores ? static_cast<Uint32>(waitSemaphores->GetSemaphoresNum()) : 0;
    submitInfo.pWaitSemaphoreInfos      = waitSemaphores ? waitSemaphores->GetSubmitInfos().data() : nullptr;
    submitInfo.commandBufferInfoCount   = static_cast<Uint32>(cmdBuffersSubmitBatch.size());
    submitInfo.pCommandBufferInfos      = cmdBuffersSubmitBatch.data();
    submitInfo.signalSemaphoreInfoCount = signalSemaphores ? static_cast<Uint32>(signalSemaphores->GetSemaphoresNum()) : 0;;
    submitInfo.pSignalSemaphoreInfos    = signalSemaphores ? signalSemaphores->GetSubmitInfos().data() : nullptr;

    VkResult submitResult = VK_SUCCESS;
    {
        SPT_PROFILER_SCOPE("vkQueueSubmit2");

        submitResult = vkQueueSubmit2(GetHandle(), 1u, &submitInfo, VK_NULL_HANDLE);
    }

    OnSubmitted(submitResult);
}

VkQueue RHIDeviceQueue::GetHandleChecked() const
{
	SPT_CHECK(IsValid());
	return GetHandle();
}

VkQueue RHIDeviceQueue::GetHandle() const
{
	return m_queueHandle;
}

void RHIDeviceQueue::OnSubmitted(VkResult submitResult)
{
    SPT_PROFILER_FUNCTION();

    if (submitResult != VK_SUCCESS)
    {
        if (submitResult == VK_ERROR_DEVICE_LOST)
        {
#if SPT_ENABLE_GPU_CRASH_DUMPS
            if (VulkanRHI::GetSettings().AreGPUCrashDumpsEnabled())
            {
                GPUCrashTracker::SaveGPUCrashDump();
            }
#endif // SPT_ENABLE_GPU_CRASH_DUMPS
        }

        SPT_CHECK_NO_ENTRY_MSG("Submitting command buffer failed with result {}", static_cast<Uint32>(submitResult));
    }
}

} // spt::vulkan
