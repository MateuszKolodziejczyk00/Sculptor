#include "ComponentsRegistry.h"


namespace spt::ecs
{

namespace Internal
{

struct RegistryInitializationData
{
	lib::HashSet<lib::RuntimeTypeInfo>      componentTypesWithInitializers;
	lib::DynamicArray<ComponentInitializer> componentInitializers;
};

using InitializersRegistry = lib::HashMap<lib::RuntimeTypeInfo, RegistryInitializationData>;

static InitializersRegistry& GetRegistry()
{
	static InitializersRegistry registry;
	return registry;
}

} // Internal


void RegisterComponentType(const lib::RuntimeTypeInfo& registryType, ComponentInitializerData componentInitializer)
{
	Internal::RegistryInitializationData& registryInitialization = Internal::GetRegistry()[registryType];

	if (registryInitialization.componentTypesWithInitializers.emplace(componentInitializer.componentType).second)
	{
		registryInitialization.componentInitializers.emplace_back(componentInitializer.componentInitializer);
	}
}

lib::Span<const ComponentInitializer> GetComponentInitializers(const lib::RuntimeTypeInfo& registryType)
{
	const auto it = Internal::GetRegistry().find(registryType);
	return it != Internal::GetRegistry().end() ? lib::Span<const ComponentInitializer>(it->second.componentInitializers) : lib::Span<const ComponentInitializer>{};
}

} // spt::ecs
