#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIFwd.h"


namespace spt::rhi
{

struct SubmitBatchData
{
	SubmitBatchData()
		: m_waitSemaphores(nullptr)
		, m_signalSemaphores(nullptr)
	{ }

	lib::DynamicArray<const RHICommandBuffer*>		m_commandBuffers;
	const RHISemaphoresArray*						m_waitSemaphores;
	const RHISemaphoresArray*						m_signalSemaphores;
};

}