#pragma once

#include "SceneRenderSystem.h"
#include "Clouds/VolumetricCloudsRenderer.h"


namespace spt::rsc
{

class RENDER_SCENE_API AtmosphereRenderSystem : public SceneRenderSystem
{
protected:

	using Super = SceneRenderSystem;

public:

	AtmosphereRenderSystem(RenderScene& owningScene);

	// Begin SceneRenderSystem overrides
	virtual void Update(const SceneUpdateContext& context) override;
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) override;
	// End SceneRenderSystem overrides

	const AtmosphereContext& GetAtmosphereContext() const { return m_atmosphereContext; }
	const AtmosphereParams& GetAtmosphereParams() const { return m_atmosphereParams; }

	const clouds::CloudsTransmittanceMap& GetCloudsTransmittanceMap() const { return m_volumetricCloudsRenderer.GetCloudsTransmittanceMap(); }

	Bool AreVolumetricCloudsEnabled() const { return m_volumetricCloudsRenderer.AreVolumetricCloudsEnabled(); }

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;

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
