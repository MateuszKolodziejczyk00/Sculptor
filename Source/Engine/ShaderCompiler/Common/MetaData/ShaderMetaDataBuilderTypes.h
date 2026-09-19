#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderDebugMetaData.h"


namespace spt::sc
{

struct ShaderCompilationMetaData
{
public:

	ShaderCompilationMetaData() = default;

	void AddShaderParam(const lib::HashedString& typeName)
	{
		SPT_CHECK(!lib::Contains(m_shaderParamsTypes, typeName));

		m_shaderParamsTypes.emplace_back(typeName);
	}

	const lib::DynamicArray<lib::HashedString>& GetShaderParamsTypes() const
	{
		return m_shaderParamsTypes;
	}

#if SPT_SHADERS_DEBUG_FEATURES
	void AddDebugLiteral(lib::HashedString literal)
	{
		debugMetaData.literals.emplace_back(literal);
	}

	const ShaderDebugMetaData& GetDebugMetaData() const
	{
		return debugMetaData;
	}
#endif // SPT_SHADERS_DEBUG_FEATURES

#if WITH_SHADERS_HOT_RELOAD
	void AddFileDependencyUnique(lib::String filePath)
	{
		if (std::find(m_fileDependencies.cbegin(), m_fileDependencies.cend(), filePath) == m_fileDependencies.cend())
		{
			m_fileDependencies.emplace_back(std::move(filePath));
		}
	}

	const lib::DynamicArray<lib::String>& GetFileDependencies() const
	{
		return m_fileDependencies;
	}
#endif // WITH_SHADERS_HOT_RELOAD


#if WITH_SHADERS_HOT_RELOAD
	lib::HashMap<lib::String, Uint64> shaderStructsVersionHashes;
#endif // WITH_SHADERS_HOT_RELOAD

private:

	lib::DynamicArray<lib::HashedString> m_shaderParamsTypes;

#if SPT_SHADERS_DEBUG_FEATURES
	ShaderDebugMetaData debugMetaData;
#endif // SPT_SHADERS_DEBUG_FEATURES

#if WITH_SHADERS_HOT_RELOAD
	lib::DynamicArray<lib::String> m_fileDependencies;
	lib::HashMap<lib::String, Uint64> m_shaderStructsVersionHashes;
#endif // WITH_SHADERS_HOT_RELOAD
};

} // spt::sc
