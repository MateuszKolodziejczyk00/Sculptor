#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIBarrierImpl.h"


namespace spt::rdr
{

class RENDERER_CORE_API Barrier
{
public:

	Barrier();

	rhi::RHIBarrier&			GetRHI();
	const rhi::RHIBarrier&		GetRHI() const;

private:

	rhi::RHIBarrier				m_rhiBarrier;
};

}