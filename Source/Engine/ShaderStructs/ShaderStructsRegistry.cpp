#include "ShaderStructsRegistry.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Priv ==========================================================================================

namespace priv
{

static lib::HashMap<lib::HashedString, ShaderStructMetaData>& GetRegistryInstance()
{
	static lib::HashMap<lib::HashedString, ShaderStructMetaData> instance;
	return instance;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderStructDefinition ========================================================================

ShaderStructMetaData::ShaderStructMetaData()
{ }

ShaderStructMetaData::ShaderStructMetaData(lib::String hlslSourceCode)
	: m_hlslSourceCode(std::move(hlslSourceCode))
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderStructsRegistry =========================================================================

void ShaderStructsRegistry::RegisterStructMetaData(lib::HashedString structName, ShaderStructMetaData structMetaData)
{
	priv::GetRegistryInstance().emplace(structName, std::move(structMetaData));
}

const ShaderStructMetaData& ShaderStructsRegistry::GetStructMetaDataChecked(lib::HashedString structName)
{
	return priv::GetRegistryInstance().at(structName);
}

const rdr::ShaderStructMetaData* ShaderStructsRegistry::GetStructMetaData(lib::HashedString structName)
{
	const auto& registry = priv::GetRegistryInstance();
	const auto it = registry.find(structName);
	return it != registry.cend() ? &it->second : nullptr;
}

} // spt::rdr
