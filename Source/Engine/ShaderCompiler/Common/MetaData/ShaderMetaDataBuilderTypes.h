#pragma once

#include "SculptorCoreTypes.h"
#include "Common/DescriptorSetCompilation/DescriptorSetCompilationDefTypes.h"


namespace spt::sc
{

namespace meta
{

static const lib::HashedString buffer		= "Buffer";
static const lib::HashedString exposeInner	= "ExposeInner";

} // meta

struct ShaderParamMetaData
{
public:

	ShaderParamMetaData() = default;

	void AddMeta(lib::HashedString metaParam)
	{
		m_metaParams.emplace(metaParam);
	}

	Bool HasMeta(lib::HashedString metaParam) const
	{
		return m_metaParams.find(metaParam) != std::cend(m_metaParams);
	}

private:

	lib::HashSet<lib::HashedString> m_metaParams;
};


struct ShaderParametersMetaData
{
public:

	ShaderParametersMetaData() = default;

	void AddParamMetaData(lib::HashedString param, const ShaderParamMetaData& metaData)
	{
		m_paramsMetaData.emplace(param, metaData);
	}

	void AddDescriptorSetMetaData(Uint32 dsIdx, const sc::DescriptorSetCompilationMetaData& metaData)
	{
		SPT_PROFILER_FUNCTION();

		const SizeType properDsIdx = static_cast<SizeType>(dsIdx);

		if (properDsIdx >= m_dsMetaData.size())
		{
			m_dsMetaData.resize(properDsIdx + 1);
		}

		m_dsMetaData[properDsIdx] = metaData;
	}

	Bool HasMeta(lib::HashedString paramName, lib::HashedString metaParam) const
	{
		const auto foundParamMetaData = m_paramsMetaData.find(paramName);
		return foundParamMetaData != std::cend(m_paramsMetaData) ? foundParamMetaData->second.HasMeta(metaParam) : false;
	}

	SizeType GetDSRegisteredHash(Uint32 descriptorSetIdx) const
	{
		const SizeType properDSIdx = static_cast<SizeType>(descriptorSetIdx);

		if (properDSIdx < m_dsMetaData.size())
		{
			return m_dsMetaData[properDSIdx].hash;
		}
		else
		{
			return idxNone<SizeType>;
		}
	}

	smd::EBindingFlags GetAdditionalBindingFlags(Uint32 descriptorSetIdx, Uint32 bindingIdx) const
	{
		const SizeType properDSIdx = static_cast<SizeType>(descriptorSetIdx);
		const SizeType properBindingIdx = static_cast<SizeType>(bindingIdx);

		if (properDSIdx < m_dsMetaData.size() && properBindingIdx < m_dsMetaData[properDSIdx].bindingsFlags.size())
		{
			return m_dsMetaData[properDSIdx].bindingsFlags[properBindingIdx];
		}
		else
		{
			return smd::EBindingFlags::None;
		}
	}

private:

	lib::HashMap<lib::HashedString, ShaderParamMetaData> m_paramsMetaData;

	lib::DynamicArray<sc::DescriptorSetCompilationMetaData> m_dsMetaData;
};

} // spt::sc
