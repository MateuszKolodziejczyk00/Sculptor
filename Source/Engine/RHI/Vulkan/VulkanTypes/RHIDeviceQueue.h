#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHICore/RHISubmitTypes.h"


namespace spt::vulkan
{

class RHI_API RHIDeviceQueue
{
public:

	RHIDeviceQueue();

	void InitializeRHI(rhi::EDeviceCommandQueueType type, VkQueue queueHandle);
	void ReleaseRHI();

	Bool IsValid() const;

	rhi::EDeviceCommandQueueType GetType() const;

	void SubmitCommands(const rhi::SubmitBatchData& submitBatch);

	// Vulkan =======================================================================

	VkQueue GetHandleChecked() const;
	VkQueue GetHandle() const;

private:

	void OnSubmitted(VkResult submitResult);

	VkQueue m_queueHandle;

	rhi::EDeviceCommandQueueType m_type;
};

} // spt::vulkan