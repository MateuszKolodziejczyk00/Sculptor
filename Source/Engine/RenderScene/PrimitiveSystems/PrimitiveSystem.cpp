#include "PrimitiveSystems/PrimitiveSystem.h"

namespace spt::rsc
{

PrimitiveSystem::PrimitiveSystem(RenderScene& owningScene)
	: m_owningScene(owningScene)
{ }

RenderScene& PrimitiveSystem::GetRenderScene() const
{
	return m_owningScene;
}

} // spt::rsc
