#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "SceneRenderingTypes.h"
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


class RENDER_SCENE_API RenderSystem : public std::enable_shared_from_this<RenderSystem>
{
public:

	RenderSystem();
	virtual ~RenderSystem() = default;

	void Initialize(RenderScene& renderScene);
	void Deinitialize(RenderScene& renderScene);

	Bool WantsUpdate() const;
	virtual void Update(RenderScene& renderScene) {};

	virtual void CollectRenderViews(const RenderScene& renderScene, const RenderView& mainRenderView, INOUT lib::DynamicArray<RenderView*>& outViews) {};

	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) {};
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) {};
	
	virtual void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) {};

	ERenderStage GetSupportedStages() const;

protected:

	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);

	ERenderStage m_supportedStages;
	Bool m_wantsUpdate;
};

} // spt::rsc