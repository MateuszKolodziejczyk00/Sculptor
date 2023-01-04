#include "RenderSystems/RenderSystem.h"

namespace spt::rsc
{

RenderSystem::RenderSystem()
	: m_wantsCallUpdate(false)
	, m_supportedStages(ERenderStage::None)
{ }

void RenderSystem::Initialize(RenderScene& renderScene, RenderSceneEntityHandle systemEntity)
{
	SPT_PROFILER_FUNCTION();

	m_systemEntity = systemEntity;

	OnInitialize(renderScene);
}

void RenderSystem::Deinitialize(RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

	OnDeinitialize(renderScene);
}

ERenderStage RenderSystem::GetSupportedStages() const
{
	return m_supportedStages;
}

RenderSceneEntityHandle RenderSystem::GetSystemEntity() const
{
	return m_systemEntity;
}

void RenderSystem::OnInitialize(RenderScene& renderScene)
{
}

void RenderSystem::OnDeinitialize(RenderScene& renderScene)
{
}

} // spt::rsc
