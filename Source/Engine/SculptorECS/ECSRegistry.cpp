#include "ECSRegistry.h"
#include "ComponentsRegistry.h"

namespace spt::ecs
{

SCULPTOR_ECS_API Registry& GetRegistry()
{
	static Registry registry;
	static bool initialized = false;

	if (!initialized)
	{
		InitializeRegistry(registry);
		initialized = true;
	}

	return registry;
}

SCULPTOR_ECS_API EntityHandle CreateEntity()
{
	Registry& registry = GetRegistry();
	const Entity entity = registry.create();
	return EntityHandle{ registry, entity };
}

} // spt::ecs
