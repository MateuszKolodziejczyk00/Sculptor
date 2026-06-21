#pragma once

#include "MaterialsMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "Utility/NamedType.h"
#include "MaterialTypes.h"
#include "ComponentsRegistry.h"
#include "MaterialShader.h"


namespace spt::mat
{

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


template<typename TMaterialData, typename TFeature>
Uint16 GetMaterialFeatureID()
{
	constexpr Uint32 featureOffset = rdr::OffsetOfType<TMaterialData, TFeature>();
	if constexpr (featureOffset == idxNone<Uint32>)
	{
		return idxNone<Uint8>;
	}
	else
	{
		return static_cast<Uint16>(featureOffset);
	}
}


struct MaterialFeatures
{
	Uint16 emissiveDataID     = idxNone<Uint16>;
	Uint16 displacementDataID = idxNone<Uint16>;
};


struct MaterialStaticParameters
{
	MaterialStaticParameters()
		: customOpacity(false)
		, doubleSided(false)
		, transparent(false)
		, emissive(false)
	{ }

	Bool IsValidMaterial() const
	{
		return shader.IsValidMaterial();
	}

	MaterialShader shader;

	MaterialFeatures features;

	Uint8 customOpacity : 1;
	Uint8 doubleSided   : 1;
	Uint8 transparent   : 1;
	Uint8 emissive      : 1;
};


BEGIN_SHADER_STRUCT(MaterialDataHandle)
	SHADER_STRUCT_FIELD(Uint16, id)
END_SHADER_STRUCT()


struct MaterialProxyComponent
{
	MaterialProxyComponent()
	{ }

	Bool SupportsRayTracing() const
	{
		return true;
	}

	MaterialDataHandle GetMaterialDataHandle() const
	{
		const Uint64 materialDataOffset = materialDataSuballocation.GetOffset();
		SPT_CHECK((materialDataOffset & constants::materialDataAlignmentMask) == 0u);
		const Uint16 materialDataID = static_cast<Uint16>(materialDataOffset >> constants::materialDataAlignmentBits);
		SPT_CHECK((Uint64(materialDataID) << constants::materialDataAlignmentBits) == materialDataOffset);
		return MaterialDataHandle{ materialDataID };
	}

	rhi::RHIVirtualAllocation materialDataSuballocation;

	MaterialStaticParameters params;
};


BEGIN_SHADER_STRUCT(RTHitGroupPermutation)
	SHADER_STRUCT_FIELD(MaterialShader, SHADER)
END_SHADER_STRUCT()

} // spt::mat
