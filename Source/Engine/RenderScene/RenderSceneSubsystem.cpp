#include "RenderSceneSubsystem.h"

namespace spt::rsc
{

RenderSceneSubsystem::RenderSceneSubsystem(RenderScene& owningScene)
	: m_owningScene(owningScene)
{ }

RenderScene& RenderSceneSubsystem::GetOwningScene() const
{
	return m_owningScene;
}

} // spt::rsc
