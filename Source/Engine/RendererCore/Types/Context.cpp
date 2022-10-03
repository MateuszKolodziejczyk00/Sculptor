#include "Context.h"
#include "RendererUtils.h"

namespace spt::rdr
{

Context::Context(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(contextDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr
