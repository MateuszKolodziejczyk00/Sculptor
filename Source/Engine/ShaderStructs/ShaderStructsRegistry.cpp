#include "ShaderStructsRegistry.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Priv ==========================================================================================

namespace priv
{

using StructsDataMap = lib::HashMap<lib::String, ShaderStructMetaData>;

StructsDataMap* instance = nullptr;

static StructsDataMap& GetRegistryInstance()
{
	if (!instance)
	{
		instance = new StructsDataMap();
	}
	return *instance;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderStructDefinition ========================================================================

ShaderStructMetaData::ShaderStructMetaData()
{ }

ShaderStructMetaData::ShaderStructMetaData(lib::String hlslSourceCode, Uint64 versionHash)
	: m_hlslSourceCode(std::move(hlslSourceCode))
	, m_versionHash(versionHash)
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderStructsRegistry =========================================================================

StructsRegistryData* ShaderStructsRegistry::GetRegistryData()
{
	return reinterpret_cast<StructsRegistryData*>(&priv::GetRegistryInstance());
}

void ShaderStructsRegistry::InitializeModule(StructsRegistryData* data)
{
	SPT_PROFILER_FUNCTION();

	priv::StructsDataMap* mainRegistry  = reinterpret_cast<priv::StructsDataMap*>(data);
	priv::StructsDataMap* localRegistry = priv::instance;
	priv::instance = mainRegistry;

	if (localRegistry)
	{
		// Register all structs from this module in the main registry
		for (const auto& [structName, structMetaData] : *localRegistry)
		{
			RegisterStructMetaData(structName, structMetaData);
		}

		if (priv::instance)
		{
			delete localRegistry; // delete this module registry, and replace it with the one that belongs to GPU api
		}
	}
}

void ShaderStructsRegistry::RegisterStructMetaData(lib::String structName, ShaderStructMetaData structMetaData)
{
	priv::GetRegistryInstance()[std::move(structName)] = std::move(structMetaData);
}

const ShaderStructMetaData& ShaderStructsRegistry::GetStructMetaDataChecked(const lib::String& structName)
{
	return priv::GetRegistryInstance().at(structName);
}

const rdr::ShaderStructMetaData* ShaderStructsRegistry::GetStructMetaData(const lib::String& structName)
{
	const auto& registry = priv::GetRegistryInstance();
	const auto it = registry.find(structName);
	return it != registry.cend() ? &it->second : nullptr;
}

} // spt::rdr
