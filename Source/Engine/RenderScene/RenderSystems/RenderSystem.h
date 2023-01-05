#pragma once

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


class RenderSystem
{
public:

	RenderSystem();
	virtual ~RenderSystem() = default;

	void Initialize(RenderScene& renderScene, RenderSceneEntityHandle systemEntity);
	void Deinitialize(RenderScene& renderScene);

	virtual void ExtractPerFrame(SceneRenderContext& context, const RenderScene& renderScene) {}
	virtual void ExtractPerView(SceneRenderContext& context, const RenderScene& renderScene, const RenderView& view) {}

	virtual void PreparePerFrame(SceneRenderContext& context, rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) {}
	virtual void PreparePerView(SceneRenderContext& context, rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const RenderView& view) {}

	ERenderStage GetSupportedStages() const;

protected:

	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);

	RenderSceneEntityHandle GetSystemEntity() const;

	Bool m_wantsCallUpdate;

	ERenderStage m_supportedStages;

private:

	RenderSceneEntityHandle m_systemEntity;
};

} // spt::rsc