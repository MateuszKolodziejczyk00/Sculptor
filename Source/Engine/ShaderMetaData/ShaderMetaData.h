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

	void						AddShaderDataParam(Uint32 setIdx, Uint32 bindingIdx, lib::HashedString paramName, ShaderDataParam dataParam);

	template<typename TShaderBindingDataType>
	void						AddShaderBindingData(Uint32 setIdx, Uint32 bindingIdx, TShaderBindingDataType bindingData);

	void						AddShaderStageToBinding(Uint32 setIdx, Uint32 bindingIdx, rhi::EShaderStage stage);

	void						SetDescriptorSetStateTypeHash(Uint32 setIdx, SizeType hash);

	void						PostInitialize();

	void						Append(const ShaderMetaData& other);

	// Queries ====================================================

	template<typename TShaderParamEntryType>
	TShaderParamEntryType		FindParamEntry(lib::HashedString paramName) const;

	ShaderParamEntryCommon		FindParamEntry(lib::HashedString paramName) const;
	
	GenericShaderBinding		GetBindingData(Uint32 setIdx, Uint32 bindingIdx) const;
	GenericShaderBinding		GetBindingData(ShaderParamEntryCommon param) const;

	Bool						ContainsBinding(Uint32 setIdx, Uint32 bindingIdx) const;

	Uint32						GetDescriptorSetsNum() const;

	const DescriptorSetArray&	GetDescriptorSets() const;

	const ShaderDescriptorSet&	GetDescriptorSet(Uint32 setIdx) const;
	
	SizeType					GetDescriptorSetHash(Uint32 setIdx) const;
	
	Uint32						FindDescriptorSetWithHash(SizeType hash) const;
	
	SizeType					GetDescriptorSetStateTypeHash(Uint32 setIdx) const;
	
private:

	void						BuildDSHashes();

	GenericShaderBinding&		GetBindingDataRef(Uint32 setIdx, Uint32 bindingIdx);

	using ShaderParameterMap	= lib::HashMap<lib::HashedString, GenericShaderParamEntry>;

	DescriptorSetArray	   		m_descriptorSets;

	ShaderParameterMap	   		m_parameterMap;

	/** Hash of descriptor set used in shader */
	lib::DynamicArray<SizeType>	m_descriptorSetParametersHashes;
	
	/** Hash of descriptor set state types used to create ds in shader */
	lib::DynamicArray<SizeType>	m_dsStateTypeHashes;

	friend srl::TypeSerializer<ShaderMetaData>;
};


template<typename TShaderParamEntryType>
void ShaderMetaData::AddShaderParamEntry(lib::HashedString paramName, TShaderParamEntryType paramEntry)
{
	m_parameterMap.emplace(paramName, paramEntry);
}

inline void ShaderMetaData::AddShaderDataParam(Uint32 setIdx, Uint32 bindingIdx, lib::HashedString paramName, ShaderDataParam dataParam)
{
	SPT_CHECK_NO_ENTRY(); // We need to figure out how to store and use raw data parameters. They may be useful for example for materials, so that each material can have custom variables
}

template<typename TShaderBindingDataType>
void ShaderMetaData::AddShaderBindingData(Uint32 setIdx, Uint32 bindingIdx, TShaderBindingDataType bindingData)
{
	const SizeType properSetIdx = static_cast<SizeType>(setIdx);

	if (properSetIdx >= m_descriptorSets.size())
	{
		m_descriptorSets.resize(properSetIdx + 1);
		m_descriptorSetParametersHashes.resize(properSetIdx + 1, idxNone<SizeType>);
		m_dsStateTypeHashes.resize(properSetIdx + 1, idxNone<SizeType>);
	}
	
	m_descriptorSets[properSetIdx].AddBinding(static_cast<Uint8>(bindingIdx), bindingData);
}

inline void ShaderMetaData::AddShaderStageToBinding(Uint32 setIdx, Uint32 bindingIdx, rhi::EShaderStage stage)
{
	GetBindingDataRef(setIdx, bindingIdx).AddShaderStage(stage);
}

inline void ShaderMetaData::SetDescriptorSetStateTypeHash(Uint32 setIdx, SizeType hash)
{
	SPT_CHECK(setIdx < m_dsStateTypeHashes.size());
	SPT_CHECK(hash != idxNone<SizeType>);
	SPT_CHECK(m_dsStateTypeHashes[setIdx] == idxNone<SizeType> || m_dsStateTypeHashes[setIdx] == hash);

	m_dsStateTypeHashes[setIdx] = hash;
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

inline void ShaderMetaData::Append(const ShaderMetaData& other)
{
	const SizeType dsNum = std::max(m_descriptorSets.size(), other.m_descriptorSets.size());
	m_dsStateTypeHashes.resize(dsNum, idxNone<SizeType>);
	m_descriptorSetParametersHashes.resize(dsNum, idxNone<SizeType>);

	for (SizeType idx = 0; idx < other.m_dsStateTypeHashes.size(); ++idx)
	{
		if (m_dsStateTypeHashes[idx] == idxNone<SizeType>)
		{
			m_dsStateTypeHashes[idx] = other.m_dsStateTypeHashes[idx];
		}
		else
		{
			SPT_CHECK(m_dsStateTypeHashes[idx] == other.m_dsStateTypeHashes[idx]);
		}
	}

	m_descriptorSets.resize(dsNum);
	for (SizeType dsIdx = 0; dsIdx < other.m_descriptorSets.size(); ++dsIdx)
	{
		m_descriptorSets[dsIdx].MergeWith(other.m_descriptorSets[dsIdx]);
	}

	std::copy(std::cbegin(other.m_parameterMap), std::cend(other.m_parameterMap), std::inserter(m_parameterMap, std::begin(m_parameterMap)));

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

inline GenericShaderBinding ShaderMetaData::GetBindingData(Uint32 setIdx, Uint32 bindingIdx) const
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

inline Bool ShaderMetaData::ContainsBinding(Uint32 setIdx, Uint32 bindingIdx) const
{
	return GetBindingData(setIdx, bindingIdx).IsValid();
}

inline Uint32 ShaderMetaData::GetDescriptorSetsNum() const
{
	return static_cast<Uint32>(m_descriptorSets.size());
}

inline const ShaderMetaData::DescriptorSetArray& ShaderMetaData::GetDescriptorSets() const
{
	return m_descriptorSets;
}

inline const ShaderDescriptorSet& ShaderMetaData::GetDescriptorSet(Uint32 setIdx) const
{
	const SizeType idx = static_cast<SizeType>(setIdx);
	return m_descriptorSets[idx];
}

inline SizeType ShaderMetaData::GetDescriptorSetHash(Uint32 setIdx) const
{
	const SizeType properSetIdx = static_cast<SizeType>(setIdx);
	SPT_CHECK(properSetIdx < m_descriptorSetParametersHashes.size());

	return m_descriptorSetParametersHashes[properSetIdx];
}

inline Uint32 ShaderMetaData::FindDescriptorSetWithHash(SizeType hash) const
{
	SPT_PROFILER_FUNCTION();
	
	for (SizeType dsIdx = 0; dsIdx < m_descriptorSetParametersHashes.size(); ++dsIdx)
	{
		const Bool hasSameType = m_dsStateTypeHashes[dsIdx] != idxNone<SizeType> && m_dsStateTypeHashes[dsIdx] == hash;
		const Bool hasSameHash = m_descriptorSetParametersHashes[dsIdx] == hash;
		if (hasSameType || hasSameHash)
		{
			return static_cast<Uint32>(dsIdx);
		}
	}

	return idxNone<Uint32>;
}

inline SizeType ShaderMetaData::GetDescriptorSetStateTypeHash(Uint32 setIdx) const
{
	const SizeType properSetIdx = static_cast<SizeType>(setIdx);
	SPT_CHECK(properSetIdx < m_dsStateTypeHashes.size());

	const Bool hasValidType = m_dsStateTypeHashes[properSetIdx];
	return hasValidType ? m_dsStateTypeHashes[properSetIdx] : m_descriptorSetParametersHashes[properSetIdx];
}

inline void ShaderMetaData::BuildDSHashes()
{
	SPT_PROFILER_FUNCTION();

	for (SizeType setIdx = 0; setIdx < m_descriptorSets.size(); ++setIdx)
	{
		SizeType hash = idxNone<SizeType>;
		
		const lib::DynamicArray<GenericShaderBinding>& bindings = m_descriptorSets[setIdx].GetBindings();
		if (!bindings.empty())
		{
			hash = 97;

			for (SizeType bindingIdx = 0; bindingIdx < bindings.size(); ++bindingIdx)
			{
				const auto bindingParam = std::find_if(std::cbegin(m_parameterMap), std::cend(m_parameterMap),
													   [setIdx, bindingIdx](const auto& param)
													   {
														   const GenericShaderParamEntry& paramEntry = param.second;
														   return paramEntry.GetSetIdx() == setIdx && paramEntry.GetBindingIdx() == bindingIdx;
													   });
				const lib::HashedString paramName = bindingParam != std::cend(m_parameterMap) ? bindingParam->first : lib::HashedString();

				lib::HashCombine(hash, HashDescriptorSetBinding(bindings[bindingIdx], paramName));
			}
		}

		SPT_CHECK(static_cast<SizeType>(setIdx) < m_descriptorSetParametersHashes.size());
		m_descriptorSetParametersHashes[setIdx] = hash;
	}
}

inline GenericShaderBinding& ShaderMetaData::GetBindingDataRef(Uint32 setIdx, Uint32 bindingIdx)
{
	const SizeType properSetIdx		= static_cast<SizeType>(setIdx);
	const SizeType properBindingIdx = static_cast<SizeType>(bindingIdx);

	SPT_CHECK(m_descriptorSets.size() > properSetIdx);
	SPT_CHECK(m_descriptorSets[properSetIdx].GetBindings().size() > properBindingIdx);

	return m_descriptorSets[properSetIdx].GetBindings()[properBindingIdx];
}

} // spt::smd