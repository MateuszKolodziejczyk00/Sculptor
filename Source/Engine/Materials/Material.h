#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RHICore/RHIAllocationTypes.h"
#include "ECSRegistry.h"
#include "Utility/NamedType.h"
#include "MaterialTypes.h"
#include "ComponentsRegistry.h"


namespace spt::mat
{

enum class EMaterialType : Flags32
{
	Invalid,
	Opaque,
	AlphaMasked
};


enum class EMaterialFlags : Flags32
{
	None     = 0,
	DoubleSided = BIT(0),
};


struct MaterialShaderSourceComponent
{
	lib::HashedString shaderPath;

	lib::DynamicArray<lib::HashedString> macroDefinitions;
};
SPT_REGISTER_COMPONENT_TYPE(MaterialShaderSourceComponent, ecs::Registry);


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
	Uint8 doubleSided   : 1;

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

	Uint16 GetMaterialDataID() const
	{
		const Uint64 materialDataOffset = materialDataSuballocation.GetOffset();
		SPT_CHECK((materialDataOffset & constants::materialDataAlignmentMask) == 0u);
		const Uint16 materialDataID = static_cast<Uint16>(materialDataOffset >> constants::materialDataAlignmentBits);
		SPT_CHECK((Uint64(materialDataID) << constants::materialDataAlignmentBits) == materialDataOffset);
		return materialDataID;
	}

	rhi::RHIVirtualAllocation materialDataSuballocation;

	MaterialShadersHash materialShadersHash;

	MaterialStaticParameters params;
};

} // spt::mat


namespace spt::lib
{

template<>
struct Hasher<mat::MaterialShadersHash>
{
	size_t operator()(const mat::MaterialShadersHash& hash) const
	{
		return hash.GetHash();
	}
};

} // spt::lib