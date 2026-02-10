#include "SceneRenderSystem.h"


namespace spt::rsc
{

SceneRenderSystem::SceneRenderSystem(RenderScene& owningScene)
	: m_supportedStages(ERenderStage::None)
	, m_owningScene(owningScene)
{ }

void SceneRenderSystem::Initialize(RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	OnInitialize(renderScene);
}

void SceneRenderSystem::Deinitialize(RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	OnDeinitialize(renderScene);
}

ERenderStage SceneRenderSystem::GetSupportedStages() const
{
	return m_supportedStages;
}

void SceneRenderSystem::OnInitialize(RenderScene& renderScene)
{
}

void SceneRenderSystem::OnDeinitialize(RenderScene& renderScene)
{
}

} // spt::rsc
