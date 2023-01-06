#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "SceneRendererTypes.h"
#include "RenderSceneRegistry.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class RenderView;


class RENDER_SCENE_API RenderSystem
{
public:

	RenderSystem();
	virtual ~RenderSystem() = default;

	void Initialize(RenderScene& renderScene, RenderSceneEntityHandle systemEntity);
	void Deinitialize(RenderScene& renderScene);

	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView) {};

	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) {};
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const RenderView& view) {};

	ERenderStage GetSupportedStages() const;
	
	RenderSceneEntityHandle GetSystemEntity() const;

protected:

	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);

	Bool m_wantsCallUpdate;

	ERenderStage m_supportedStages;

private:

	RenderSceneEntityHandle m_systemEntity;
};

} // spt::rsc