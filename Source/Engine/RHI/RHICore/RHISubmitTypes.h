#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIFwd.h"


namespace spt::rhi
{

struct SubmitBatchData
{
	SubmitBatchData()
		: waitSemaphores(nullptr)
		, signalSemaphores(nullptr)
	{ }

	lib::DynamicArray<const RHICommandBuffer*>		commandBuffers;
	const RHISemaphoresArray*						waitSemaphores;
	const RHISemaphoresArray*						signalSemaphores;
};

}