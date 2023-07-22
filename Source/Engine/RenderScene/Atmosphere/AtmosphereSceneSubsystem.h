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

	// Begin RenderSceneSubsystem overrides
	virtual void Update() override;
	// End RenderSceneSubsystem overrides

	const AtmosphereContext& GetAtmosphereContext() const;

	Bool IsAtmosphereTextureDirty() const;

private:

	void InitializeResources();

	void OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);

	void UpdateAtmosphereContext();

	AtmosphereContext m_atmosphereContext;

	AtmosphereParams m_atmosphereParams;

	Bool m_isAtmosphereContextDirty;
	Bool m_isAtmosphereTextureDirty;
};

} // spt::rsc
