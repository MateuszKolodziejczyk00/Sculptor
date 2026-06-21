#pragma once

#include "SculptorCoreTypes.h"
#include "SceneRendererTypes.h"
#include "Utils/SceneRenderingTypes.h"


namespace spt::rsc
{

class ViewRenderingSpec;

struct PlacementExe;


class PlacementSystemBacked
{
public:

	PlacementSystemBacked();

	void Initialize(lib::MemoryArena& arena, RenderScene& renderScene);
	void Deinitialize(RenderScene& renderScene);
	void ProcessPlacements(PlacementProcessor processor, void* customData);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);

private:

	lib::StaticArray<PlacementExe*, 4u> m_placementExecutions;
};

} // spt::rsc
