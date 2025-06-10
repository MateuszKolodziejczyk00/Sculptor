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

	AtmosphereRenderSystem();

	// Begin SceneRenderSystem overrides
	virtual void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, const lib::DynamicArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) override;
	// End SceneRenderSystem overrides

	const clouds::CloudsTransmittanceMap& GetCloudsTransmittanceMap() const { return m_volumetricCloudsRenderer.GetCloudsTransmittanceMap(); }

	Bool AreVolumetricCloudsEnabled() const { return m_volumetricCloudsRenderer.AreVolumetricCloudsEnabled(); }

private:

	void RenderPerView(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec);

	void RenderAerialPerspective(rg::RenderGraphBuilder& graphBuilder, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const RenderViewEntryContext& context) const;

	clouds::VolumetricCloudsRenderer m_volumetricCloudsRenderer;
};

} // spt::rsc
