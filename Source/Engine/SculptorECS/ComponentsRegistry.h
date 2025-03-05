#pragma once

#include "SculptorECS.h"
#include "SculptorECSMacros.h"
#include "SculptorCoreTypes.h"
#include "Utility/Templates/Callable.h"


namespace spt::ecs
{

namespace Internal
{

template<typename TRegistry, typename TComponent>
void InitializeComponent(void* registryData)
{
	TRegistry& registry = *static_cast<TRegistry*>(registryData);
	registry.storage<TComponent>();
}

} // Internal


using ComponentInitializer = lib::RawCallable<void(void*)>;


struct ComponentInitializerData
{
	const lib::RuntimeTypeInfo componentType;
	ComponentInitializer       componentInitializer;
};


SCULPTOR_ECS_API void RegisterComponentType(const lib::RuntimeTypeInfo& registryType, ComponentInitializerData componentInitializer);
SCULPTOR_ECS_API lib::Span<const ComponentInitializer> GetComponentInitializers(const lib::RuntimeTypeInfo& registryType);


template<typename TRegistry>
void InitializeRegistry(TRegistry& registry)
{
	const lib::RuntimeTypeInfo registryType = lib::TypeInfo<TRegistry>();
	const lib::Span<const ComponentInitializer> componentInitializers = GetComponentInitializers(registryType);

	for (const ComponentInitializer componentInitializer : componentInitializers)
	{
		componentInitializer(&registry);
	}
}


template<typename TRegistry, typename TComponent>
void RegisterComponentType()
{
	const lib::RuntimeTypeInfo registryType = lib::TypeInfo<TRegistry>();

	const ComponentInitializer componentInitializer = Internal::InitializeComponent<TRegistry, TComponent>;

	const ComponentInitializerData componentInitializerData = { lib::TypeInfo<TComponent>(), componentInitializer };

	RegisterComponentType(registryType, componentInitializerData);
}


template<typename TComponent, typename... TRegistries>
class ComponentAutoRegistrator
{
public:

	ComponentAutoRegistrator()
	{
		(RegisterComponentType<TRegistries, TComponent>(), ...);
	}
};


#define SPT_REGISTER_COMPONENT_TYPE(TComponent, ...) \
	inline spt::ecs::ComponentAutoRegistrator<TComponent, __VA_ARGS__> SPT_SCOPE_NAME_EVAL(autoRegistrator, __LINE__);

} // spt::ecs
