#pragma once

#include "SculptorCoreTypes.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefTypes.h"
#include "ShaderDebugMetaData.h"


namespace spt::sc
{

struct ShaderCompilationMetaData
{
public:

	ShaderCompilationMetaData() = default;

	void AddDescriptorSetMetaData(SizeType dsIdx, const sc::DescriptorSetCompilationMetaData& metaData)
	{
		SPT_PROFILER_FUNCTION();

		if (dsIdx >= m_dsMetaData.size())
		{
			m_dsMetaData.resize(dsIdx + 1);
		}

		m_dsMetaData[dsIdx] = metaData;
	}

	SizeType GetDSTypeID(SizeType descriptorSetIdx) const
	{
		if (descriptorSetIdx < m_dsMetaData.size())
		{
			return m_dsMetaData[descriptorSetIdx].typeID;
		}
		else
		{
			return idxNone<SizeType>;
		}
	}

	SizeType GetDescriptorSetsNum() const
	{
		return m_dsMetaData.size();
	}

	void SetShaderParamsTypeName(const lib::HashedString& name)
	{
		SPT_CHECK(!m_shaderParamsTypeName.IsValid());
		m_shaderParamsTypeName = name;
	}

	const lib::HashedString& GetShaderParamsTypeName() const
	{
		return m_shaderParamsTypeName;
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

private:

	lib::DynamicArray<sc::DescriptorSetCompilationMetaData> m_dsMetaData;

	lib::HashedString m_shaderParamsTypeName;

#if SPT_SHADERS_DEBUG_FEATURES
	ShaderDebugMetaData debugMetaData;
#endif // SPT_SHADERS_DEBUG_FEATURES

#if WITH_SHADERS_HOT_RELOAD
	lib::DynamicArray<lib::String> m_fileDependencies;
#endif // WITH_SHADERS_HOT_RELOAD
};

} // spt::sc
