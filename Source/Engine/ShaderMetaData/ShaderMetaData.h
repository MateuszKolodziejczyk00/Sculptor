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

	template<typename TShaderParamEntryType>
	void						AddShaderParamEntry(lib::HashedString paramName, TShaderParamEntryType paramEntry);

	template<typename TShaderBindingDataType>
	void						AddShaderBindingData(Uint8 setIdx, Uint8 bindingIdx, TShaderBindingDataType bindingData);

	void						AddShaderStageToBinding(Uint8 setIdx, Uint8 bindingIdx, rhi::EShaderStage stage);

	template<typename TShaderParamEntryType>
	TShaderParamEntryType		FindParamEntry(lib::HashedString paramName) const;

	
	GenericShaderBinding		GetBindingData(Uint8 setIdx, Uint8 bindingIdx) const;

	Bool						ContainsBinding(Uint8 setIdx, Uint8 bindingIdx) const;

	Uint8						GetDescriptorSetsNum() const;

	const DescriptorSetArray&	GetDescriptorSets() const;

	const ShaderDescriptorSet&	GetDescriptorSet(Uint8 setIdx) const;
	
private:

	GenericShaderBinding&		GetBindingDataRef(Uint8 setIdx, Uint8 bindingIdx);

	using ShaderParameterMap	= lib::HashMap<lib::HashedString, GenericShaderParamEntry>;

	DescriptorSetArray	   		m_descriptorSets;

	ShaderParameterMap	   		m_parameterMap;

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

template<typename TShaderParamEntryType>
TShaderParamEntryType ShaderMetaData::FindParamEntry(lib::HashedString paramName) const
{
	const auto foundParam = m_parameterMap.find(paramName);
	if (foundParam)
	{
		return foundParam->second.GetOrDefault<TShaderParamEntryType>();
	}

	return TShaderParamEntryType();
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

} // spt::smd