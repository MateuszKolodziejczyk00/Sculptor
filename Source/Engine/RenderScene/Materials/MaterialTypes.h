#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RHICore/RHIBufferTypes.h"


namespace spt::rsc
{

enum class EMaterialType
{
	Opaque,
	AlphaMasked,
	Transparent
};

struct MaterialsDataComponent
{
	lib::DynamicArray<RenderingDataEntityHandle> materials;
};

struct MaterialCommonData
{
	MaterialCommonData() = default;
	
	explicit MaterialCommonData(rhi::RHISuballocation inMaterialDataSuballocation)
		: materialDataSuballocation(inMaterialDataSuballocation)
		, materialType(EMaterialType::Opaque)
	{ }

	bool SupportsRayTracing() const
	{
		return materialType == EMaterialType::Opaque;
	}

	rhi::RHISuballocation materialDataSuballocation;

	EMaterialType materialType;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, MaterialPBRData)
	SHADER_STRUCT_FIELD(Uint32, baseColorTextureIdx)
	SHADER_STRUCT_FIELD(Uint32, metallicRoughnessTextureIdx)
	SHADER_STRUCT_FIELD(Uint32, normalsTextureIdx)
	SHADER_STRUCT_FIELD(Real32, metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, baseColorFactor)
	SHADER_STRUCT_FIELD(Real32, roughnessFactor)
END_SHADER_STRUCT();

} // spt::rsc