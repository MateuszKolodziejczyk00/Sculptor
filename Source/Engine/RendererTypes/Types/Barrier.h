#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBarrierImpl.h"


namespace spt::renderer
{

class RENDERER_TYPES_API Barrier
{
public:

	Barrier();

	rhi::RHIBarrier&			GetRHI();
	const rhi::RHIBarrier&		GetRHI() const;

private:

	rhi::RHIBarrier				m_rhiBarrier;
};

}