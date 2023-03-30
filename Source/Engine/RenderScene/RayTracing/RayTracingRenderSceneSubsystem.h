#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderSceneSubsystem.h"

namespace spt::rdr
{
class TopLevelAS;
} // spt::rsc


namespace spt::rsc
{

class RENDER_SCENE_API RayTracingRenderSceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit RayTracingRenderSceneSubsystem(RenderScene& owningScene);

	// Begin RenderSceneSubsystem overrides
	virtual void Update() override;
	// End RenderSceneSubsystem overrides

	const lib::SharedPtr<rdr::TopLevelAS>& GetSceneTLAS() const { return m_tlas; }

private:

	void UpdateTLAS();

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;
};

} // spt::rsc