#include "RenderScene.h"

namespace spt::rsc
{

RenderScene::RenderScene()
	: m_registry(nullptr)
{ }

void RenderScene::Initialize(entt::registry& registry)
{
	m_registry = &registry;
}

entt::registry& RenderScene::GetRegistry() const
{
	SPT_CHECK(!!m_registry);
	return *m_registry;
}

} // spt::rsc
