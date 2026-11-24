#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::rsc
{

class RENDER_SCENE_API LightsRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::LightsSystem;

	explicit LightsRenderSystem(lib::MemoryArena& arena, RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void UpdateGPUSceneData(const SceneUpdateContext& context, RenderSceneConstants& sceneData);
	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	// End SceneRenderSystem overrides

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void BuildLightsTiles(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context);

	lib::SharedPtr<rdr::Buffer> m_spotLightProxyVertices;
	lib::SharedPtr<rdr::Buffer> m_pointLightProxyVertices;

	lib::SharedPtr<rdr::Buffer> m_lightsDrawCommandsBuffer;

	lib::SharedPtr<rdr::Buffer> m_sceneLocalLightsBuffer;
};

} // spt::rsc
