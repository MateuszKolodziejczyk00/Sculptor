#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderMetaDataTypes.h"

namespace spt::srl
{
template<typename TType>
struct TypeSerializer;
}

namespace spt::smd
{

class ShaderMetaData
{
public:

	using DescriptorSetArray = lib::DynamicArray<ShaderDescriptorSet>;

	ShaderMetaData() = default;

	// Initialization =============================================

	template<typename TShaderParamEntryType>
	void						AddShaderParamEntry(lib::HashedString paramName, TShaderParamEntryType paramEntry);

	template<typename TShaderBindingDataType>
	void						AddShaderBindingData(Uint8 setIdx, Uint8 bindingIdx, TShaderBindingDataType bindingData);

	void						AddShaderStageToBinding(Uint8 setIdx, Uint8 bindingIdx, rhi::EShaderStage stage);

	void						PostInitialize();

	// Queries ====================================================

	template<typename TShaderParamEntryType>
	TShaderParamEntryType		FindParamEntry(lib::HashedString paramName) const;

	ShaderParamEntryCommon		FindParamEntry(lib::HashedString paramName) const;
	
	GenericShaderBinding		GetBindingData(Uint8 setIdx, Uint8 bindingIdx) const;
	GenericShaderBinding		GetBindingData(ShaderParamEntryCommon param) const;

	Bool						ContainsBinding(Uint8 setIdx, Uint8 bindingIdx) const;

	Uint8						GetDescriptorSetsNum() const;

	const DescriptorSetArray&	GetDescriptorSets() const;

	const ShaderDescriptorSet&	GetDescriptorSet(Uint8 setIdx) const;
	
	SizeType					GetDescriptorSetHash(Uint8 setIdx) const;
	
private:

	void						BuildDSHashes();

	GenericShaderBinding&		GetBindingDataRef(Uint8 setIdx, Uint8 bindingIdx);

	using ShaderParameterMap	= lib::HashMap<lib::HashedString, GenericShaderParamEntry>;

	DescriptorSetArray	   		m_descriptorSets;

	ShaderParameterMap	   		m_parameterMap;

	lib::DynamicArray<SizeType>	m_descriptorSetHashes;

	friend srl::TypeSerializer<ShaderMetaData>;
};


template<typename TShaderParamEntryType>
void ShaderMetaData::AddShaderParamEntry(lib::HashedString paramName, TShaderParamEntryType paramEntry)
{
	m_parameterMap.emplace(paramName, paramEntry);
}

template<typename TShaderBindingDataType>
void ShaderMetaData::AddShaderBindingData(Uint8 setIdx, Uint8 bindingIdx, TShaderBindingDataType bindingData)
{
	const SizeType properSetIdx = static_cast<SizeType>(setIdx);

	if (properSetIdx >= m_descriptorSets.size())
	{
		m_descriptorSets.resize(properSetIdx + 1);
	}
	
	m_descriptorSets[properSetIdx].AddBinding(bindingIdx, bindingData);
}

inline void ShaderMetaData::AddShaderStageToBinding(Uint8 setIdx, Uint8 bindingIdx, rhi::EShaderStage stage)
{
	GetBindingDataRef(setIdx, bindingIdx).AddShaderStage(stage);
}

inline void ShaderMetaData::PostInitialize()
{
	std::for_each(std::begin(m_descriptorSets), std::end(m_descriptorSets),
				  [](ShaderDescriptorSet& set)
				  {
					  set.PostInitialize();
				  });
	
	BuildDSHashes();
}

template<typename TShaderParamEntryType>
TShaderParamEntryType ShaderMetaData::FindParamEntry(lib::HashedString paramName) const
{
	const auto foundParam = m_parameterMap.find(paramName);
	if (foundParam != std::cend(m_parameterMap))
	{
		return foundParam->second.GetOrDefault<TShaderParamEntryType>();
	}

	return TShaderParamEntryType();
}

inline ShaderParamEntryCommon ShaderMetaData::FindParamEntry(lib::HashedString paramName) const
{
	const auto foundParam = m_parameterMap.find(paramName);
	return foundParam != std::cend(m_parameterMap) ? foundParam->second.GetCommonData() : ShaderParamEntryCommon();
}

inline GenericShaderBinding ShaderMetaData::GetBindingData(Uint8 setIdx, Uint8 bindingIdx) const
{
	const SizeType properSetIdx = static_cast<SizeType>(setIdx);

	if (m_descriptorSets.size() > properSetIdx)
	{
		const SizeType properBindingIdx = static_cast<SizeType>(bindingIdx);
		
		if (m_descriptorSets[properSetIdx].GetBindings().size() > properBindingIdx)
		{
			return m_descriptorSets[properSetIdx].GetBindings()[properBindingIdx];
		}
	}
	
	return GenericShaderBinding();
}

inline GenericShaderBinding ShaderMetaData::GetBindingData(ShaderParamEntryCommon param) const
{
	return GetBindingData(param.setIdx, param.bindingIdx);
}

inline Bool ShaderMetaData::ContainsBinding(Uint8 setIdx, Uint8 bindingIdx) const
{
	return GetBindingData(setIdx, bindingIdx).IsValid();
}

inline Uint8 ShaderMetaData::GetDescriptorSetsNum() const
{
	return static_cast<Uint8>(m_descriptorSets.size());
}

inline const ShaderMetaData::DescriptorSetArray& ShaderMetaData::GetDescriptorSets() const
{
	return m_descriptorSets;
}

inline const ShaderDescriptorSet& ShaderMetaData::GetDescriptorSet(Uint8 setIdx) const
{
	const SizeType idx = static_cast<SizeType>(setIdx);
	return m_descriptorSets[idx];
}

inline GenericShaderBinding& ShaderMetaData::GetBindingDataRef(Uint8 setIdx, Uint8 bindingIdx)
{
	const SizeType properSetIdx		= static_cast<SizeType>(setIdx);
	const SizeType properBindingIdx = static_cast<SizeType>(bindingIdx);

	SPT_CHECK(m_descriptorSets.size() > properSetIdx);
	SPT_CHECK(m_descriptorSets[properSetIdx].GetBindings().size() > properBindingIdx);

	return m_descriptorSets[properSetIdx].GetBindings()[properBindingIdx];
}

inline void ShaderMetaData::BuildDSHashes()
{
	SPT_CHECK(m_descriptorSets.size() <= maxValue<Uint8>);
	const Uint8 setsNum = static_cast<Uint8>(m_descriptorSets.size());

	for (Uint8 setIdx = 0; setIdx < setsNum; ++setIdx)
	{
		SizeType hash = m_descriptorSets[setIdx].Hash();

		for (const auto& [paramName, paramEntry] : m_parameterMap)
		{
			if (paramEntry.GetSetIdx() == setIdx)
			{
				lib::HashCombine(hash, lib::GetHash(paramName));
				lib::HashCombine(hash, paramEntry.Hash());
			}
		}

		SPT_CHECK(m_descriptorSetHashes.size() == static_cast<SizeType>(setIdx));
		m_descriptorSetHashes.emplace_back(hash);
	}
}

inline SizeType ShaderMetaData::GetDescriptorSetHash(Uint8 setIdx) const
{
	const SizeType properSetIdx = static_cast<SizeType>(setIdx);
	SPT_CHECK(properSetIdx < m_descriptorSetHashes.size());

	return m_descriptorSetHashes[properSetIdx];
}

} // spt::smd