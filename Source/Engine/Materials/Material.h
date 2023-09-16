#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RHICore/RHIBufferTypes.h"
#include "ECSRegistry.h"
#include "Utility/NamedType.h"


namespace spt::mat
{

enum class EMaterialType : Uint32
{
	Invalid,
	Opaque,
	AlphaMasked
};


struct MaterialShaderSourceComponent
{
	lib::HashedString shaderPath;

	lib::DynamicArray<lib::HashedString> macroDefinitions;
};


class MaterialShadersHash
{
public:

	MaterialShadersHash()
		: m_hash(0u)
	{ }

	explicit MaterialShadersHash(Uint64 hash)
		: m_hash(hash)
	{ }

	MaterialShadersHash& operator=(const MaterialShadersHash& other)
	{
		m_hash = other.m_hash;
		return *this;
	}

	Bool operator==(const MaterialShadersHash& other) const
	{
		return m_hash == other.m_hash;
	}

	Uint64 GetHash() const
	{
		return m_hash;
	}

private:

	Uint64 m_hash;
};


struct MaterialStaticParameters
{
	MaterialStaticParameters()
		: materialType(EMaterialType::Invalid)
		, customOpacity(false)
	{ }

	MaterialShadersHash GenerateShaderID() const
	{
		const Uint64 hash = lib::HashCombine(static_cast<SizeType>(materialShaderHandle.entity()),
											 static_cast<SizeType>(materialType),
											 materialDataStructName,
											 customOpacity);

		return MaterialShadersHash(hash);
	}

	EMaterialType materialType;

	lib::HashedString materialDataStructName;

	ecs::EntityHandle materialShaderHandle;

	Uint8 customOpacity : 1;
};


struct MaterialProxyComponent
{
	MaterialProxyComponent()
		: materialShadersHash(0u)
	{ }

	Bool SupportsRayTracing() const
	{
		return true;
	}

	rhi::RHISuballocation materialDataSuballocation;

	MaterialShadersHash materialShadersHash;

	MaterialStaticParameters params;
};


struct MaterialSlotsComponent
{
	lib::DynamicArray<ecs::EntityHandle> slots;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, MaterialPBRData)
	SHADER_STRUCT_FIELD(Uint32, baseColorTextureIdx)
	SHADER_STRUCT_FIELD(Uint32, metallicRoughnessTextureIdx)
	SHADER_STRUCT_FIELD(Uint32, normalsTextureIdx)
	SHADER_STRUCT_FIELD(Real32, metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, baseColorFactor)
	SHADER_STRUCT_FIELD(Real32, roughnessFactor)
END_SHADER_STRUCT();

} // spt::mat


namespace std
{

template<>
struct hash<spt::mat::MaterialShadersHash>
{
	size_t operator()(const spt::mat::MaterialShadersHash& hash) const
	{
		return hash.GetHash();
	}
};

} // std