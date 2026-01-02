#pragma once

#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDependencyImpl.h"


namespace spt::rdr
{

class Texture;
class GPUWorkload;


class GPUResourceInitQueue
{
public:

	GPUResourceInitQueue();

	void EnqueueTextureInit(const lib::SharedPtr<Texture>& texture);

	lib::SharedPtr<GPUWorkload> RecordGPUInitializations();

private:

	lib::Spinlock m_lock;
	rhi::RHIDependency m_dependency;
};

} // spt::rdr