#pragma once

#include "SceneRenderSystem.h"
#include "StaticMeshDepthPrepassRenderer.h"
#include "StaticMeshForwardOpaqueRenderer.h"
#include "StaticMeshShadowMapRenderer.h"

namespace spt::rsc
{

class RENDER_SCENE_API StaticMeshesRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:
	
	StaticMeshesRenderSystem();

	// Begin SceneRenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) override;
	virtual void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) override;
	// End SceneRenderSystem overrides

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	StaticMeshShadowMapRenderer m_shadowMapRenderer;

	StaticMeshDepthPrepassRenderer m_depthPrepassRenderer;
	
	StaticMeshForwardOpaqueRenderer m_forwardOpaqueRenderer;
};

} // spt::rsc
