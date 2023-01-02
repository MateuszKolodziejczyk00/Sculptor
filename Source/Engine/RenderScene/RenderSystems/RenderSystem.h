#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

class RenderScene;

class RenderSystem
{
public:

	RenderSystem();

	virtual void Initialize(RenderScene& renderScene);

	virtual void Update(const RenderScene& renderScene, float dt);

	Bool WantsCallUpdate() const;

protected:

	Bool bWantsCallUpdate;
};

} // spt::rsc