#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "SceneRenderer/SceneRenderingTypes.h"
#include "View/ViewRenderingSpec.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class RenderView;
struct RenderSceneConstants;


struct SceneUpdateContext
{
	const RenderView&            mainRenderView;
	const SceneRendererSettings& rendererSettings;
};


class RENDER_SCENE_API SceneRenderSystem : public std::enable_shared_from_this<SceneRenderSystem>
{
public:

	SceneRenderSystem(RenderScene& owningScene);
	virtual ~SceneRenderSystem() = default;

	void Initialize(RenderScene& renderScene);
	void Deinitialize(RenderScene& renderScene);

	virtual void Update(const SceneUpdateContext& context) {};

	virtual void UpdateGPUSceneData(RenderSceneConstants& sceneData) {}

	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector) {};

	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) {};
	
	virtual void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) {};

	ERenderStage GetSupportedStages() const;

protected:

	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);

	RenderScene& GetOwningScene() const { return m_owningScene; }

	ERenderStage m_supportedStages;

private:

	RenderScene& m_owningScene;
};

} // spt::rsc
