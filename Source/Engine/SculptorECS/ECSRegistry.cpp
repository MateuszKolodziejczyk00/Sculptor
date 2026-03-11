#include "ECSRegistry.h"
#include "ComponentsRegistry.h"
#include "Engine.h"

namespace spt::ecs
{

struct RegistryInstance
{
	RegistryInstance()
	{
		InitializeRegistry(registry);
	}

	Registry registry;
};


SCULPTOR_ECS_API Registry& GetRegistry()
{
	static engn::TEngineSingleton<RegistryInstance> instance;
	return instance.Get().registry;
}

SCULPTOR_ECS_API EntityHandle CreateEntity()
{
	Registry& registry = GetRegistry();
	const Entity entity = registry.create();
	return EntityHandle{ registry, entity };
}

} // spt::ecs
