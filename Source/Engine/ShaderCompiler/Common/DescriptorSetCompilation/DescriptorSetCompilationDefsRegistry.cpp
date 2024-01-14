#include "DescriptorSetCompilationDefsRegistry.h"

#include <regex>

namespace spt::sc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Priv ==========================================================================================

namespace priv
{

static lib::HashMap<lib::HashedString, DescriptorSetCompilationDef>& GetRegistryInstance()
{
	static lib::HashMap<lib::HashedString, DescriptorSetCompilationDef> instance;
	return instance;
}

} // priv

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetCompilationDef ===================================================================

DescriptorSetCompilationDef::DescriptorSetCompilationDef()
{ }

DescriptorSetCompilationDef::DescriptorSetCompilationDef(const lib::String& shaderCode, DescriptorSetCompilationMetaData metaData)
	: m_compilationMetaData(std::move(metaData))
{
	SetShaderCode(shaderCode);
}

lib::String DescriptorSetCompilationDef::GetShaderCode(Uint32 dsIdx) const
{
	SPT_PROFILER_FUNCTION();

	lib::String resultShaderCode = m_shaderCode;

	std::for_each(std::cbegin(m_dsIdxPositions), std::cend(m_dsIdxPositions),
				  [&resultShaderCode, dsIdx](SizeType idxPosition)
				  {
					  std::string idxString = std::to_string(dsIdx);
					  SPT_CHECK(idxString.size() <= 2); // We don't support indices larger than 99
					  if (idxString.size() == 1)
					  {
						  idxString = " " + idxString;
					  }
					  resultShaderCode.replace(idxPosition, 2, idxString);
				  });

	return resultShaderCode;
}

const DescriptorSetCompilationMetaData& DescriptorSetCompilationDef::GetMetaData() const
{
	return m_compilationMetaData;
}

void DescriptorSetCompilationDef::SetShaderCode(lib::StringView code)
{
	m_shaderCode = code;
	BuildDSIdxPositionsArray();
}

void DescriptorSetCompilationDef::BuildDSIdxPositionsArray()
{
	SPT_PROFILER_FUNCTION();

	// clear previous indices - every time we find all of them
	m_dsIdxPositions.clear();

	static const std::regex dsIdxRegex(R"~(vk::binding\(\d+\s*,\s*XX\s*\))~");
		
	auto dsIndicesIt = std::sregex_iterator(std::cbegin(m_shaderCode), std::cend(m_shaderCode), dsIdxRegex);

	SPT_CHECK(dsIndicesIt != std::sregex_iterator()); // no bindings found. Probably shouldn't be ever intended

	for (; dsIndicesIt != std::sregex_iterator(); ++dsIndicesIt)
	{
		const std::smatch dsIdxMatch = *dsIndicesIt;
		const lib::String dsIdxString = dsIdxMatch.str();

		const SizeType dsIdxPosition = m_shaderCode.find("XX", dsIndicesIt->position()); // find "XX" for which set idx should be substituted
		SPT_CHECK(dsIdxPosition != lib::String::npos);

		m_dsIdxPositions.push_back(dsIdxPosition);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetCompilationDefsRegistry ==========================================================

DescriptorSetCompilationDefsRegistry::DescriptorSetCompilationDefsRegistry()
{ }

void DescriptorSetCompilationDefsRegistry::RegisterDSCompilationDef(const lib::HashedString& dsName, const DescriptorSetCompilationDef& definition)
{
	priv::GetRegistryInstance().emplace(dsName, definition);
}

const DescriptorSetCompilationDef& DescriptorSetCompilationDefsRegistry::GetDescriptorSetCompilationDef(const lib::HashedString& dsName)
{
	return priv::GetRegistryInstance().at(dsName);
}

lib::String DescriptorSetCompilationDefsRegistry::GetDescriptorSetShaderSourceCode(const lib::HashedString& dsName, Uint32 dsIdx)
{
	return GetDescriptorSetCompilationDef(dsName).GetShaderCode(dsIdx);
}

} // spt::sc
