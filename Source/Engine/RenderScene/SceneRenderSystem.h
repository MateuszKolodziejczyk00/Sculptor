#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "RenderSceneRegistry.h"
#include "View/ViewRenderingSpec.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class RenderView;


class RENDER_SCENE_API SceneRenderSystem : public std::enable_shared_from_this<SceneRenderSystem>
{
public:

	SceneRenderSystem();
	virtual ~SceneRenderSystem() = default;

	void Initialize(RenderScene& renderScene);
	void Deinitialize(RenderScene& renderScene);

	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector) {};

	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) {};
	
	virtual void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) {};

	ERenderStage GetSupportedStages() const;

protected:

	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);

	ERenderStage m_supportedStages;
};

} // spt::rsc