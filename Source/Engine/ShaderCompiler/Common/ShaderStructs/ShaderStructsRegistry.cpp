#include "ShaderStructsRegistry.h"

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Priv ==========================================================================================

namespace priv
{

static lib::HashMap<lib::HashedString, ShaderStructDefinition>& GetRegistryInstance()
{
	static lib::HashMap<lib::HashedString, ShaderStructDefinition> instance;
	return instance;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderStructDefinition ========================================================================

ShaderStructDefinition::ShaderStructDefinition()
{ }

ShaderStructDefinition::ShaderStructDefinition(lib::String sourceCode)
	: m_sourceCode(std::move(sourceCode))
{ }

const lib::String& ShaderStructDefinition::GetSourceCode() const
{
	return m_sourceCode;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ShaderStructsRegistry =========================================================================

void ShaderStructsRegistry::RegisterStructDefinition(lib::HashedString structName, const ShaderStructDefinition& structDef)
{
	priv::GetRegistryInstance().emplace(structName, structDef);
}

const ShaderStructDefinition& ShaderStructsRegistry::GetStructDefinition(lib::HashedString structName)
{
	return priv::GetRegistryInstance().at(structName);
}

} // spt::sc
