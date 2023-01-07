#include "PrimitivesSystem.h"

namespace spt::rsc
{

PrimitivesSystem::PrimitivesSystem(RenderScene& owningScene)
	: m_owningScene(owningScene)
{ }

RenderScene& PrimitivesSystem::GetOwningScene() const
{
	return m_owningScene;
}

} // spt::rsc
