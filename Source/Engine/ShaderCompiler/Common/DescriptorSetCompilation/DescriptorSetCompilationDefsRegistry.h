#pragma once

#include "Common/ShaderCompilerMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::sc
{

class DescriptorSetCompilationDef
{
public:

	DescriptorSetCompilationDef();
	explicit DescriptorSetCompilationDef(const lib::String& shaderCode);

	lib::String GetShaderCode(Uint32 dsIdx) const;

	void SetShaderCode(lib::StringView code);

private:

	void BuildDSIdxPositionsArray();

	lib::String					m_shaderCode;
	lib::DynamicArray<SizeType>	m_dsIdxPositions;
};


class DescriptorSetCompilationDefsRegistry
{
public:

	static void RegisterDSCompilationDef(const lib::HashedString& dsName, const DescriptorSetCompilationDef& definition);

	static const DescriptorSetCompilationDef& GetDescriptorSetCompilationDef(const lib::HashedString& dsName);

private:

	DescriptorSetCompilationDefsRegistry();

	lib::HashMap<lib::HashedString, DescriptorSetCompilationDef> m_dsStateTypeToDefinition;
};

} // spt::sc