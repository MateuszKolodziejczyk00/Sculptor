#pragma once

#include "SculptorCoreTypes.h"
#include "SceneRendererTypes.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

class RenderScene;


class RenderSystem
{
public:

	RenderSystem();
	virtual ~RenderSystem() = default;

	void Initialize(RenderScene& renderScene, RenderSceneEntityHandle systemEntity);
	void Deinitialize(RenderScene& renderScene);

	ERenderStage GetSupportedStages() const;

protected:

	virtual void OnInitialize(RenderScene& renderScene);
	virtual void OnDeinitialize(RenderScene& renderScene);

	RenderSceneEntityHandle GetSystemEntity() const;

	Bool m_wantsCallUpdate;

	ERenderStage m_supportedStages;

private:

	RenderSceneEntityHandle m_systemEntity;
};

} // spt::rsc