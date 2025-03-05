#include "DirectionalLightShadowMasksRenderStage.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "Lights/ViewShadingInput.h"


namespace spt::rsc
{
REGISTER_RENDER_STAGE(ERenderStage::DirectionalLightsShadowMasks, DirectionalLightShadowMasksRenderStage);


DirectionalLightShadowMasksRenderStage::DirectionalLightShadowMasksRenderStage()
{ }

void DirectionalLightShadowMasksRenderStage::BeginFrame(const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	Base::BeginFrame(renderScene, viewSpec);

	PrepareRenderers(renderScene, viewSpec);

	for (auto& [entity, renderer] : m_shadowMaskRenderers)
	{
		renderer.BeginFrame(renderScene, viewSpec);
	}
}

void DirectionalLightShadowMasksRenderStage::OnRender(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, RenderStageExecutionContext& stageContext)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(rdr::Renderer::IsRayTracingEnabled());

	ViewDirectionalShadowMasksData& frameShadowMasks = viewSpec.GetBlackboard().Create<ViewDirectionalShadowMasksData>();
	for (auto& [entity, renderer] : m_shadowMaskRenderers)
	{
		frameShadowMasks.shadowMasks[entity] = renderer.Render(graphBuilder, renderScene, viewSpec);
	}


	GetStageEntries(viewSpec).BroadcastOnRenderStage(graphBuilder, renderScene, viewSpec, stageContext);
}

void DirectionalLightShadowMasksRenderStage::PrepareRenderers(const RenderScene& renderScene, ViewRenderingSpec& viewSpec)
{
	SPT_PROFILER_FUNCTION();

	const rsc::RenderSceneRegistry& registry = renderScene.GetRegistry();

	// Remove renderers for removed lights
	{
		lib::DynamicArray<RenderSceneEntity> lightsToRemove;

		for (const auto& [entityID, renderer] : m_shadowMaskRenderers)
		{
			if (!registry.valid(entityID))
			{
				lightsToRemove.emplace_back(entityID);
			}
		}

		for (RenderSceneEntity lightToRemove : lightsToRemove)
		{
			m_shadowMaskRenderers.erase(lightToRemove);
		}
	}

	// Add renderers for new lights
	const auto directionalLights = registry.view<DirectionalLightData>();
	for (const auto& [entity, directionalLight] : directionalLights.each())
	{
		if (m_shadowMaskRenderers.find(entity) == m_shadowMaskRenderers.end())
		{
			RTShadowMaskRenderer& renderer = m_shadowMaskRenderers[entity];
			renderer.Initialize(entity);
		}
	}
}

} // spt::rsc
