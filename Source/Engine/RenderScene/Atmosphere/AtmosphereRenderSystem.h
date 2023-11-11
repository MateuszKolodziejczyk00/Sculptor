#pragma once

#include "SceneRenderSystem.h"


namespace spt::rsc
{

class RENDER_SCENE_API AtmosphereRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	AtmosphereRenderSystem();

	// Begin SceneRenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs) override;
	// End SceneRenderSystem overrides

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void ApplyAtmosphereToView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData);
};

} // spt::rsc
