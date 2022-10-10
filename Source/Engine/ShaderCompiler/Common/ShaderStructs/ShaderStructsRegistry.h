#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class SHADER_COMPILER_API ShaderStructDefinition
{
public:

	ShaderStructDefinition();

	explicit ShaderStructDefinition(lib::String sourceCode);

	const lib::String& GetSourceCode() const;

private:

	lib::String m_sourceCode;
};


class SHADER_COMPILER_API ShaderStructsRegistry
{
public:

	static void RegisterStructDefinition(lib::HashedString structName, const ShaderStructDefinition& structDef);
	static const ShaderStructDefinition& GetStructDefinition(lib::HashedString structName);

private:

	ShaderStructsRegistry() = default;
};

} // spt::sc