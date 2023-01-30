#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

struct MaterialCommonData
{
	MaterialCommonData() = default;
	
	explicit MaterialCommonData(rhi::RHISuballocation inMaterialDataSuballocation)
		: materialDataSuballocation(inMaterialDataSuballocation)
	{ }

	rhi::RHISuballocation materialDataSuballocation;
};


BEGIN_ALIGNED_SHADER_STRUCT(, 16, MaterialPBRData)
	SHADER_STRUCT_FIELD(Uint32, baseColorTextureIdx)
	SHADER_STRUCT_FIELD(Uint32, metallicRoughnessTextureIdx)
	SHADER_STRUCT_FIELD(Uint32, normalsTextureIdx)
	SHADER_STRUCT_FIELD(Real32, metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, baseColorFactor)
	SHADER_STRUCT_FIELD(Real32, roughnessFactor)
END_SHADER_STRUCT();

} // spt::rsc