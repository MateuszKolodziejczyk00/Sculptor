#pragma once

#include "SceneRenderSystems/SceneRenderSystem.h"
#include "Clouds/VolumetricCloudsRenderer.h"


namespace spt::rsc
{

class RENDER_SCENE_API AtmosphereRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	static constexpr ESceneRenderSystem systemType = ESceneRenderSystem::AtmosphereSystem;

	AtmosphereRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	void Update(const SceneUpdateContext& context);
	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings);
	// End SceneRenderSystem overrides

	const AtmosphereContext& GetAtmosphereContext() const { return m_atmosphereContext; }
	const AtmosphereParams& GetAtmosphereParams() const { return m_atmosphereParams; }

	const clouds::CloudsTransmittanceMap& GetCloudsTransmittanceMap() const { return m_volumetricCloudsRenderer.GetCloudsTransmittanceMap(); }

	Bool AreVolumetricCloudsEnabled() const { return m_volumetricCloudsRenderer.AreVolumetricCloudsEnabled(); }

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;

	void InitializeResources();

	void OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity);

	void UpdateAtmosphereContext();

	void UpdateDirectionalLightIlluminance(RenderSceneEntity entity);

	clouds::VolumetricCloudsRenderer m_volumetricCloudsRenderer;

	AtmosphereContext m_atmosphereContext;

	AtmosphereParams m_atmosphereParams;

	Bool m_isAtmosphereContextDirty;
	Bool m_isAtmosphereTextureDirty;

	Bool m_shouldUpdateTransmittanceLUT;;

	math::Vector3f m_transmittanceAtZenith;
};

} // spt::rsc
