#include "PrefabEntity.h"


namespace spt::as
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// PrefabEntityTypesRegistry =====================================================================

using PrefabEntityTypesRegistry = lib::HashMap<lib::RuntimeTypeInfo, PrefabEntityTypeMetaData>;


static PrefabEntityTypesRegistry& GetInstance()
{
	static PrefabEntityTypesRegistry instance;
	return instance;
}


void RegisterPrefabType(const lib::RuntimeTypeInfo& type, const PrefabEntityTypeMetaData& factory)
{
	GetInstance()[type] = factory;
}


const PrefabEntityTypeMetaData* GetPrefabType(const lib::RuntimeTypeInfo& type)
{
	const PrefabEntityTypesRegistry& registry = GetInstance();
	const auto foundIt = registry.find(type);
	return foundIt != registry.cend() ? &foundIt->second : nullptr;
}

} // spt::as
