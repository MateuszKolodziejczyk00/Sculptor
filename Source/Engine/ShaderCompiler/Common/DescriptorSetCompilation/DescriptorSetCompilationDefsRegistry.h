#pragma once

#include "SculptorCoreTypes.h"
#include "DescriptorSetCompilationDefTypes.h"


namespace spt::sc
{

class DescriptorSetCompilationDef
{
public:

	DescriptorSetCompilationDef();
	explicit DescriptorSetCompilationDef(const lib::String& shaderCode, DescriptorSetCompilationMetaData metaData);

	lib::String GetShaderCode(Uint32 dsIdx) const;

	const DescriptorSetCompilationMetaData& GetMetaData() const;

	void SetShaderCode(lib::StringView code);

private:

	void BuildDSIdxPositionsArray();

	lib::String							m_shaderCode;
	lib::DynamicArray<SizeType>			m_dsIdxPositions;
	DescriptorSetCompilationMetaData	m_compilationMetaData;
};


class DescriptorSetCompilationDefsRegistry
{
public:

	static void RegisterDSCompilationDef(const lib::HashedString& dsName, const DescriptorSetCompilationDef& definition);

	static const DescriptorSetCompilationDef& GetDescriptorSetCompilationDef(const lib::HashedString& dsName);
	static lib::String GetDescriptorSetShaderSourceCode(const lib::HashedString& dsName, Uint32 dsIdx);

private:

	DescriptorSetCompilationDefsRegistry();

	lib::HashMap<lib::HashedString, DescriptorSetCompilationDef> m_dsStateTypeToDefinition;
};

} // spt::sc