#pragma once

#include "SculptorCoreTypes.h"

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

	ShaderMetaData() = default;

	// Initialization =============================================

	void SetDescriptorSetStateTypeID(SizeType setIdx, SizeType typeID);

	void Append(const ShaderMetaData& other);

	// Queries ====================================================
	
	Uint32   GetDescriptorSetsNum() const;

	Bool     HasValidDescriptorSetAtIndex(SizeType setIdx) const;
	
	Uint32   FindDescriptorSetOfType(SizeType typeID) const;
	
	SizeType GetDescriptorSetStateTypeID(SizeType setIdx) const;
	
private:

	/** Hash of descriptor set state types used to create ds in shader */
	lib::DynamicArray<SizeType>	m_dsStateTypeIDs;

	friend srl::TypeSerializer<ShaderMetaData>;
};


inline void ShaderMetaData::SetDescriptorSetStateTypeID(SizeType setIdx, SizeType typeID)
{
	if (typeID != idxNone<SizeType>)
	{
		if (m_dsStateTypeIDs.size() <= setIdx)
		{
			m_dsStateTypeIDs.resize(setIdx + 1, idxNone<SizeType>);
		}

		SPT_CHECK(m_dsStateTypeIDs[setIdx] == idxNone<SizeType> || m_dsStateTypeIDs[setIdx] == typeID);
		m_dsStateTypeIDs[setIdx] = typeID;
	}
}

inline void ShaderMetaData::Append(const ShaderMetaData& other)
{
	const SizeType dsNum = std::max(m_dsStateTypeIDs.size(), other.m_dsStateTypeIDs.size());
	m_dsStateTypeIDs.resize(dsNum, idxNone<SizeType>);

	for (SizeType idx = 0; idx < other.m_dsStateTypeIDs.size(); ++idx)
	{
		if (m_dsStateTypeIDs[idx] == idxNone<SizeType>)
		{
			m_dsStateTypeIDs[idx] = other.m_dsStateTypeIDs[idx];
		}
		else
		{
			SPT_CHECK_MSG(m_dsStateTypeIDs[idx] == other.m_dsStateTypeIDs[idx], "Not matching Descriptor States at idx {}", idx);
		}
	}
}

inline Uint32 ShaderMetaData::GetDescriptorSetsNum() const
{
	return static_cast<Uint32>(m_dsStateTypeIDs.size());
}

inline Bool ShaderMetaData::HasValidDescriptorSetAtIndex(SizeType setIdx) const
{
	return setIdx < m_dsStateTypeIDs.size() && m_dsStateTypeIDs[setIdx] != idxNone<SizeType>;
}

inline Uint32 ShaderMetaData::FindDescriptorSetOfType(SizeType typeID) const
{
	SPT_PROFILER_FUNCTION();
	
	const auto foundDS = std::find(m_dsStateTypeIDs.cbegin(), m_dsStateTypeIDs.cend(), typeID);
	return foundDS != m_dsStateTypeIDs.cend() ? static_cast<Uint32>(std::distance(m_dsStateTypeIDs.cbegin(), foundDS)) : idxNone<Uint32>;
}

inline SizeType ShaderMetaData::GetDescriptorSetStateTypeID(SizeType setIdx) const
{
	SPT_CHECK(setIdx < m_dsStateTypeIDs.size());

	return m_dsStateTypeIDs[setIdx];
}

} // spt::smd