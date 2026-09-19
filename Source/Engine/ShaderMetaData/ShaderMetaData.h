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

	ShaderMetaData() = default;

	// Initialization =============================================

	void SetShaderParamsTypes(lib::DynamicArray<lib::HashedString> types);

	void Append(const ShaderMetaData& other);

#if WITH_SHADERS_HOT_RELOAD
	void RegisterShaderStruct(const lib::String& structName, Uint64 versionHash);
#endif // WITH_SHADERS_HOT_RELOAD

	// Queries ====================================================

	Bool     HasShaderParam(const lib::HashedString& paramType) const;

	const lib::DynamicArray<lib::HashedString>& GetShaderParamsTypes() const;

#if WITH_SHADERS_HOT_RELOAD
	const lib::DynamicArray<ShaderStructVersion>& GetShaderStructsVersionHashes() const;
#endif // WITH_SHADERS_HOT_RELOAD

	// Serialization =============================================

	void Serialize(srl::Serializer& serializer);
	
private:

	lib::DynamicArray<lib::HashedString> m_shaderParamsTypes;

#if WITH_SHADERS_HOT_RELOAD
	lib::DynamicArray<ShaderStructVersion> shaderStructsVersionHashes;
#endif // WITH_SHADERS_HOT_RELOAD
};


inline void ShaderMetaData::SetShaderParamsTypes(lib::DynamicArray<lib::HashedString> types)
{
	SPT_CHECK(m_shaderParamsTypes.empty());
	m_shaderParamsTypes = std::move(types);
}

inline void ShaderMetaData::Append(const ShaderMetaData& other)
{
	if (m_shaderParamsTypes.empty())
	{
		m_shaderParamsTypes = other.m_shaderParamsTypes;
	}
	else
	{
		SPT_CHECK(m_shaderParamsTypes.size() == other.m_shaderParamsTypes.size());
		for (SizeType idx = 0; idx < m_shaderParamsTypes.size(); ++idx)
		{
			SPT_CHECK_MSG(m_shaderParamsTypes[idx] == other.m_shaderParamsTypes[idx], "Not matching Shader Params at idx {}", idx);
		}
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

inline Bool ShaderMetaData::HasShaderParam(const lib::HashedString& paramType) const
{
	return lib::Contains(m_shaderParamsTypes, paramType);
}

inline const lib::DynamicArray<lib::HashedString>& ShaderMetaData::GetShaderParamsTypes() const
{
	return m_shaderParamsTypes;
}

#if WITH_SHADERS_HOT_RELOAD
inline const lib::DynamicArray<ShaderStructVersion>& ShaderMetaData::GetShaderStructsVersionHashes() const
{
	return shaderStructsVersionHashes;
}
#endif // WITH_SHADERS_HOT_RELOAD

inline void ShaderMetaData::Serialize(srl::Serializer& serializer)
{
	serializer.Serialize("ShaderParamsTypes", m_shaderParamsTypes);
#if WITH_SHADERS_HOT_RELOAD
	serializer.Serialize("ShaderStructsVersionHashes", shaderStructsVersionHashes);
#endif // WITH_SHADERS_HOT_RELOAD
}

} // spt::smd
