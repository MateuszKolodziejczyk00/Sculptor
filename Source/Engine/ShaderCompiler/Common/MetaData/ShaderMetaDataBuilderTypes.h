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

	const rhi::SamplerDefinition& GetImmutableSamplerDefinition(Uint32 descriptorSetIdx, Uint32 bindingIdx) const
	{
		const SizeType properDSIdx = static_cast<SizeType>(descriptorSetIdx);

		SPT_CHECK(properDSIdx < m_dsMetaData.size());
		return m_dsMetaData[properDSIdx].bindingToImmutableSampler.at(bindingIdx);
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

private:

	lib::DynamicArray<sc::DescriptorSetCompilationMetaData> m_dsMetaData;

#if SPT_SHADERS_DEBUG_FEATURES
	ShaderDebugMetaData debugMetaData;
#endif // SPT_SHADERS_DEBUG_FEATURES
};

} // spt::sc
