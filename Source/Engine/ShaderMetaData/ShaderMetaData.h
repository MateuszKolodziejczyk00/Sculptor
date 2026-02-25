#pragma once

#include "SculptorCoreTypes.h"
#include "Serialization.h"

namespace spt::srl
{
template<typename TType>
struct TypeSerializer;
}

namespace spt::smd
{

struct ShaderStructVersion
{
	lib::String structName;
	Uint64      versionHash = 0u;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("StructName", structName);
		serializer.Serialize("VersionHash", versionHash);
	}
};


class ShaderMetaData
{
public:

	ShaderMetaData()
	{
		m_dsStateTypeIDs.emplace_back(idxNone<SizeType>); // reserve idx 0 for bindless descriptor set
	}

	// Initialization =============================================

	void SetDescriptorSetStateTypeID(SizeType setIdx, SizeType typeID);

	void SetShaderParamsTypeName(const lib::HashedString& name);

	void Append(const ShaderMetaData& other);

#if WITH_SHADERS_HOT_RELOAD
	void RegisterShaderStruct(const lib::String& structName, Uint64 versionHash);
#endif // WITH_SHADERS_HOT_RELOAD

	// Queries ====================================================
	
	Uint32   GetDescriptorSetsNum() const;

	Bool     HasValidDescriptorSetAtIndex(SizeType setIdx) const;
	
	Uint32   FindDescriptorSetOfType(SizeType typeID) const;
	
	SizeType GetDescriptorSetStateTypeID(SizeType setIdx) const;

	const lib::HashedString& GetShaderParamsType() const;

#if WITH_SHADERS_HOT_RELOAD
	const lib::DynamicArray<ShaderStructVersion>& GetShaderStructsVersionHashes() const;
#endif // WITH_SHADERS_HOT_RELOAD

	// Serialization =============================================

	void Serialize(srl::Serializer& serializer);
	
private:

	/** Hash of descriptor set state types used to create ds in shader */
	lib::DynamicArray<SizeType>	m_dsStateTypeIDs;

	lib::HashedString m_shaderParamsTypeName;

#if WITH_SHADERS_HOT_RELOAD
	lib::DynamicArray<ShaderStructVersion> shaderStructsVersionHashes;
#endif // WITH_SHADERS_HOT_RELOAD
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

inline void ShaderMetaData::SetShaderParamsTypeName(const lib::HashedString& name)
{
	SPT_CHECK(!m_shaderParamsTypeName.IsValid());
	m_shaderParamsTypeName = name;
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

	if (!m_shaderParamsTypeName.IsValid())
	{
		m_shaderParamsTypeName = other.m_shaderParamsTypeName;
	}
	else
	{
		SPT_CHECK(!other.m_shaderParamsTypeName.IsValid() || other.m_shaderParamsTypeName == m_shaderParamsTypeName);
	}

#if WITH_SHADERS_HOT_RELOAD
	for (const auto& shaderStructVersion : other.shaderStructsVersionHashes)
	{
		const auto existingStructVersionIt = std::find_if(shaderStructsVersionHashes.cbegin(), shaderStructsVersionHashes.cend(), [&shaderStructVersion](const ShaderStructVersion& existingStructVersion) { return existingStructVersion.structName == shaderStructVersion.structName; });
		if (existingStructVersionIt == shaderStructsVersionHashes.cend())
		{
			shaderStructsVersionHashes.emplace_back(shaderStructVersion);
		}
		else
		{
			SPT_CHECK_MSG(existingStructVersionIt->versionHash == shaderStructVersion.versionHash, "Not matching version hashes for shader struct '{}'", shaderStructVersion.structName);
		}
	}
#endif // WITH_SHADERS_HOT_RELOAD


	SPT_CHECK_MSG(GetDescriptorSetStateTypeID(0u) == idxNone<Uint64>, "Bindless shaders cannot be used with explicit descriptor set at idx 0");
}

#if WITH_SHADERS_HOT_RELOAD
inline void ShaderMetaData::RegisterShaderStruct(const lib::String& structName, Uint64 versionHash)
{
	if (std::find_if(shaderStructsVersionHashes.cbegin(), shaderStructsVersionHashes.cend(), [&structName](const ShaderStructVersion& structVersion) { return structVersion.structName == structName; }) == shaderStructsVersionHashes.cend())
	{
		shaderStructsVersionHashes.emplace_back(ShaderStructVersion{ structName, versionHash });
	}
}
#endif // WITH_SHADERS_HOT_RELOAD

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
	const auto foundDS = std::find(m_dsStateTypeIDs.cbegin(), m_dsStateTypeIDs.cend(), typeID);
	return foundDS != m_dsStateTypeIDs.cend() ? static_cast<Uint32>(std::distance(m_dsStateTypeIDs.cbegin(), foundDS)) : idxNone<Uint32>;
}

inline SizeType ShaderMetaData::GetDescriptorSetStateTypeID(SizeType setIdx) const
{
	SPT_CHECK(setIdx < m_dsStateTypeIDs.size());

	return m_dsStateTypeIDs[setIdx];
}

inline const lib::HashedString& ShaderMetaData::GetShaderParamsType() const
{
	return m_shaderParamsTypeName;
}

#if WITH_SHADERS_HOT_RELOAD
inline const lib::DynamicArray<ShaderStructVersion>& ShaderMetaData::GetShaderStructsVersionHashes() const
{
	return shaderStructsVersionHashes;
}
#endif // WITH_SHADERS_HOT_RELOAD

inline void ShaderMetaData::Serialize(srl::Serializer& serializer)
{
	serializer.Serialize("DescriptorSets", m_dsStateTypeIDs);
	serializer.Serialize("ShaderParamsTypeName", m_shaderParamsTypeName);
#if WITH_SHADERS_HOT_RELOAD
	serializer.Serialize("ShaderStructsVersionHashes", shaderStructsVersionHashes);
#endif // WITH_SHADERS_HOT_RELOAD
}

} // spt::smd
