#pragma once

#include "RenderSceneSubsystem.h"
#include "AtmosphereTypes.h"
#include "RenderSceneRegistry.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

class RENDER_SCENE_API AtmosphereSceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit AtmosphereSceneSubsystem(RenderScene& owningScene);
	~AtmosphereSceneSubsystem();

	// Begin RenderSceneSubsystem overrides
	virtual void Update() override;
	// End RenderSceneSubsystem overrides

	const AtmosphereContext& GetAtmosphereContext() const { return m_atmosphereContext; }

	const AtmosphereParams& GetAtmosphereParams() const { return m_atmosphereParams; }

	Bool IsAtmosphereTextureDirty() const;

	Bool ShouldUpdateTransmittanceLUT() const;
	void PostUpdateTransmittanceLUT();

private:

	void InitializeResources();

	void OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);
	void OnDirectionalLightRemoved(RenderSceneRegistry& registry, RenderSceneEntity entity);

	void UpdateAtmosphereContext();

	void UpdateDirectionalLightIlluminance(RenderSceneEntity entity);

	AtmosphereContext m_atmosphereContext;

	AtmosphereParams m_atmosphereParams;

	Bool m_isAtmosphereContextDirty;
	Bool m_isAtmosphereTextureDirty;

	Bool m_shouldUpdateTransmittanceLUT;;

	math::Vector3f m_transmittanceAtZenith;
};

} // spt::rsc
