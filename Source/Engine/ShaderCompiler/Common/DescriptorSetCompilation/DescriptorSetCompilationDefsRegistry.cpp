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

DescriptorSetCompilationDef::DescriptorSetCompilationDef(const lib::String& shaderCode)
{
	SetShaderCode(shaderCode);
}

lib::String DescriptorSetCompilationDef::GetShaderCode(Uint32 dsIdx) const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dsIdx <= 9); // currently only single-digit indices are supported

	lib::String resultShaderCode = m_shaderCode;

	std::for_each(std::cbegin(m_dsIdxPositions), std::cend(m_dsIdxPositions),
				  [&resultShaderCode, dsIdx](SizeType idxPosition)
				  {
					  resultShaderCode[idxPosition] = ('0' + static_cast<char>(dsIdx));
				  });

	return resultShaderCode;
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

	static const std::regex dsIdxRegex(R"~(vk::binding(\d+\s*,\s*X\s*))~");
		
	auto dsIndicesIt = std::sregex_iterator(std::cbegin(m_shaderCode), std::cend(m_shaderCode), dsIdxRegex);

	SPT_CHECK(dsIndicesIt != std::sregex_iterator()); // no bindings found. Probably shouldn't be ever intended

	while (dsIndicesIt != std::sregex_iterator())
	{
		const std::smatch dsIdxMatch = *dsIndicesIt;
		const lib::String dsIdxString = dsIdxMatch.str();

		const SizeType dsIdxPosition = m_shaderCode.find_first_of('X', dsIndicesIt->prefix().length()); // find 'X' for which set idx should be substituted
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

} // spt::sc
