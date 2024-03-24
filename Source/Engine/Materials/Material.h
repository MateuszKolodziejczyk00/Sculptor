#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RHICore/RHIAllocationTypes.h"
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

	static constexpr MaterialShadersHash NoMaterial()
	{
		return MaterialShadersHash();
	}

	constexpr MaterialShadersHash()
		: m_hash(idxNone<Uint64>)
	{ }

	explicit constexpr MaterialShadersHash(Uint64 hash)
		: m_hash(hash)
	{
		SPT_CHECK(hash != idxNone<Uint64>);
	}

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

	Bool IsValid() const
	{
		return m_hash != idxNone<Uint64>;
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

	Bool IsValidMaterial() const
	{
		return materialType != EMaterialType::Invalid;
	}

	MaterialShadersHash GenerateShaderID() const
	{
		const Uint64 hash = lib::HashCombine(static_cast<SizeType>(materialShaderHandle.entity()),
											 static_cast<SizeType>(materialType),
											 materialDataStructName,
											 customOpacity);

		return MaterialShadersHash(hash);
	}

	EMaterialType materialType;

	Uint8 customOpacity : 1;

	lib::HashedString materialDataStructName;

	ecs::EntityHandle materialShaderHandle;
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

	rhi::RHIVirtualAllocation materialDataSuballocation;

	MaterialShadersHash materialShadersHash;

	MaterialStaticParameters params;
};


struct MaterialSlotsComponent
{
	lib::DynamicArray<ecs::EntityHandle> slots;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, MaterialPBRData)
	SHADER_STRUCT_FIELD(Uint32,         baseColorTextureIdx)
	SHADER_STRUCT_FIELD(Uint32,         metallicRoughnessTextureIdx)
	SHADER_STRUCT_FIELD(Uint32,         normalsTextureIdx)
	SHADER_STRUCT_FIELD(Real32,         metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, baseColorFactor)
	SHADER_STRUCT_FIELD(Real32,         roughnessFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, emissiveFactor)
	SHADER_STRUCT_FIELD(Uint32,         emissiveTextureIdx)
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