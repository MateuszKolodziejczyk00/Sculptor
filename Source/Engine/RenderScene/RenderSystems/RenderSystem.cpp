#include "RenderSystems/RenderSystem.h"

namespace spt::rsc
{

RenderSystem::RenderSystem()
	: bWantsCallUpdate(false)
{ }

void RenderSystem::Initialize(RenderScene& renderScene)
{

}

void RenderSystem::Update(const RenderScene& renderScene, float dt)
{
}

Bool RenderSystem::WantsCallUpdate() const
{
	return bWantsCallUpdate;
}

} // spt::rsc
