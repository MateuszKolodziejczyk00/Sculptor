#include "Types/GPUEvent.h"

namespace spt::rdr
{

GPUEvent::GPUEvent(const RendererResourceName& name, const rhi::EventDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(definition);
	GetRHI().SetName(name.Get());
}

} // spt::rdr
