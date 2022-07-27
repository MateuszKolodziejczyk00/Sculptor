#pragma once

#include "SculptorCoreTypes.h"
#include "RHIFwd.h"


namespace spt::rhi
{

struct SubmitBatchData
{
	SubmitBatchData()
		: m_waitSemaphores(nullptr)
		, m_signalSemaphores(nullptr)
	{ }

	lib::DynamicArray<const rhi::RHICommandBuffer*>		m_commandBuffers;
	const rhi::RHISemaphoresArray*						m_waitSemaphores;
	const rhi::RHISemaphoresArray*						m_signalSemaphores;
};

}