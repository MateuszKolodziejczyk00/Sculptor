#include "RenderGraphManager.h"
#include "ProfilerCore.h"
#include "RenderGraphResourcesPool.h"

namespace spt::rg
{

void RenderGraphManager::OnBeginFrame()
{
	SPT_PROFILER_FUNCTION();

	RenderGraphResourcesPool::Get().OnBeginFrame();
}

} // spt::rg
