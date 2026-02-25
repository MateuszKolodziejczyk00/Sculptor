#pragma once

#include "SculptorCoreTypes.h"
#include "DescriptorSetCompilationDefTypes.h"


namespace spt::sc
{

class DescriptorSetCompilationDef
{
public:

	DescriptorSetCompilationDef();
	explicit DescriptorSetCompilationDef(lib::String shaderCode, lib::String accessorsCode, DescriptorSetCompilationMetaData metaData);

	lib::String GetShaderCode(Uint32 dsIdx) const;

	const DescriptorSetCompilationMetaData& GetMetaData() const;

	void SetShaderCode(lib::String code);

	const lib::String& GetAccessorsCode() const { return m_accessorsCode; }

private:

	void BuildDSIdxPositionsArray();

	lib::String							m_shaderCode;
	lib::String							m_accessorsCode;
	lib::DynamicArray<SizeType>			m_dsIdxPositions;
	DescriptorSetCompilationMetaData	m_compilationMetaData;
};


class DescriptorSetCompilationDefsRegistry
{
public:

	static void RegisterDSCompilationDef(lib::String dsName, const DescriptorSetCompilationDef& definition);

	static const DescriptorSetCompilationDef& GetDescriptorSetCompilationDef(const lib::String& dsName);
	static lib::String GetDescriptorSetShaderSourceCode(const lib::String& dsName, Uint32 dsIdx);

private:

	DescriptorSetCompilationDefsRegistry();

	lib::HashMap<lib::HashedString, DescriptorSetCompilationDef> m_dsStateTypeToDefinition;
};

} // spt::sc
