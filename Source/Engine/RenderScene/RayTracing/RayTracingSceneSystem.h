#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "PrimitivesSystem.h"

namespace spt::rdr
{
class TopLevelAS;
} // spt::rsc


namespace spt::rsc
{

class RENDER_SCENE_API RayTracingSceneSystem : public PrimitivesSystem
{
protected:

	using Super = PrimitivesSystem;

public:

	explicit RayTracingSceneSystem(RenderScene& owningScene);

	virtual void Update() override;

	const lib::SharedPtr<rdr::TopLevelAS>& GetSceneTLAS() const { return m_tlas; }

private:

	void UpdateTLAS();

	lib::SharedPtr<rdr::TopLevelAS> m_tlas;
};

} // spt::rsc