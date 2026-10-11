#pragma once

#include "SculptorCoreTypes.h"
#include "GPUApi.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcesPool.h"
#include "Utils/TransfersUtils.h"


namespace spt::tools
{

template<typename TBuildFunc>
void ExecuteRenderGraph(TBuildFunc&& buildFunc)
{
	SPT_PROFILER_FUNCTION();

	lib::MemoryArena arena("Tools Render Graph Arena", 1024u * 1024u, 512u * 1024u * 1024u);

	rg::RenderGraphResourcesPool resourcesPool;

	{
		rg::RenderGraphBuilder graphBuilder(arena, resourcesPool);

		buildFunc(graphBuilder);

		rdr::FlushPendingUploads();

		graphBuilder.Execute();

		rdr::GPUApi::WaitIdle();
	}
}

} // spt::tools
