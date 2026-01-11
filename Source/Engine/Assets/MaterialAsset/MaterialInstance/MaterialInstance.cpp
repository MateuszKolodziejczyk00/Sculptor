#include "MaterialInstance.h"
#pragma optimize("", off)

namespace spt::as
{

void MaterialInstanceTypesRegistry::RegisterMaterialInstanceType(const lib::RuntimeTypeInfo& type, MaterialInstanceFactory factory)
{
	MaterialInstanceTypesRegistry& registry = GetInstance();
	registry.m_factories[type] = factory;
}

lib::UniquePtr<MaterialInstance> MaterialInstanceTypesRegistry::CreateMaterialInstance(const lib::RuntimeTypeInfo& type)
{
	MaterialInstanceTypesRegistry& registry = GetInstance();

	const auto foundFactory = registry.m_factories.find(type);
	if (foundFactory != registry.m_factories.cend())
	{
		return foundFactory->second();
	}

	return nullptr;
}

MaterialInstanceTypesRegistry& MaterialInstanceTypesRegistry::GetInstance()
{
	static MaterialInstanceTypesRegistry instance;
	return instance;
}

} // spt::as
