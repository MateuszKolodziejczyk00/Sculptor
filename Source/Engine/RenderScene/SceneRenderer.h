#pragma once

#include "RenderStages/BasePassRenderStage.h"


namespace spt::rsc
{

class SceneRenderer
{
public:

	SceneRenderer();

	const BasePassRenderStage& GetBasePassStage() const;

private:

	BasePassRenderStage m_basePass;
};

} // spt::rsc