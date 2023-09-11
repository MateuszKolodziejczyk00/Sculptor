#include "ECSRegistry.h"

namespace spt::ecs
{

SCULPTOR_ECS_API Registry& GetRegistry()
{
	static Registry registry;
	return registry;
}

SCULPTOR_ECS_API EntityHandle CreateEntity()
{
	Registry& registry = GetRegistry();
	const Entity entity = registry.create();
	return EntityHandle{ registry, entity };
}

} // spt::ecs
