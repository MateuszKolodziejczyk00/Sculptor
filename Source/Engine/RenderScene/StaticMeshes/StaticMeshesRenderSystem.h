#pragma once

#include "RenderSystem.h"
#include "StaticMeshDepthPrepassRenderer.h"
#include "StaticMeshForwardOpaqueRenderer.h"
#include "StaticMeshShadowMapRenderer.h"

namespace spt::rsc
{

class RENDER_SCENE_API StaticMeshesRenderSystem : public RenderSystem
{
protected:

	using Super = RenderSystem;

public:
	
	StaticMeshesRenderSystem();

	// Begin RenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene) override;
	virtual void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) override;
	// End RenderSystem overrides

private:

	StaticMeshShadowMapRenderer m_shadowMapRenderer;

	StaticMeshDepthPrepassRenderer m_depthPrepassRenderer;
	
	StaticMeshForwardOpaqueRenderer m_forwardOpaqueRenderer;
};

} // spt::rsc
