#pragma once

#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::mat
{

BEGIN_SHADER_STRUCT(EmissiveMaterialData)
	SHADER_STRUCT_FIELD(math::Vector3f,                         emissiveFactor)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector3f>, emissiveTexture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(MaterialDepthData)
	SHADER_STRUCT_FIELD(Real32,                         maxDepthCm)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>, depthTexture)
END_SHADER_STRUCT();

} // spt::mat
