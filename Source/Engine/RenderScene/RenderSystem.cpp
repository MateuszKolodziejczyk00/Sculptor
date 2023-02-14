#include "RenderSystem.h"

namespace spt::rsc
{

RenderSystem::RenderSystem()
	: m_supportedStages(ERenderStage::None)
	, m_wantsUpdate(false)
{ }

void RenderSystem::Initialize(RenderScene& renderScene)
{
	SPT_PROFILER_FUNCTION();

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

void RenderSystem::OnInitialize(RenderScene& renderScene)
{
}

void RenderSystem::OnDeinitialize(RenderScene& renderScene)
{
}

Bool RenderSystem::WantsUpdate() const
{
	return m_wantsUpdate;
}

} // spt::rsc
