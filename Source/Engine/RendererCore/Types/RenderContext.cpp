#include "RenderContext.h"
#include "RendererUtils.h"

namespace spt::rdr
{

RenderContext::RenderContext(const RendererResourceName& name, const rhi::ContextDefinition& contextDef)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(contextDef);
	GetRHI().SetName(name.Get());
}

} // spt::rdr
