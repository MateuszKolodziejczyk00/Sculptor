#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rdr
{
class Buffer;
class TextureView;
} // spt::rdr


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(AtmosphereParams)
	SHADER_STRUCT_FIELD(Uint32, directionalLightsNum)
	
	SHADER_STRUCT_FIELD(float, groundRadiusMM)
	SHADER_STRUCT_FIELD(float, atmosphereRadiusMM)

	SHADER_STRUCT_FIELD(math::Vector3f, groundAlbedo)

	SHADER_STRUCT_FIELD(math::Vector3f, rayleighScattering)
	SHADER_STRUCT_FIELD(float, rayleighAbsorption)
	
	SHADER_STRUCT_FIELD(float, mieScattering)
	SHADER_STRUCT_FIELD(float, mieAbsorption)

	SHADER_STRUCT_FIELD(math::Vector3f, ozoneAbsorption)
END_SHADER_STRUCT();


struct AtmosphereContext
{
	lib::SharedPtr<rdr::Buffer>			atmosphereParamsBuffer;

	lib::SharedPtr<rdr::Buffer>			directionalLightsBuffer;

	lib::SharedPtr<rdr::TextureView>	transmittanceLUT;

	lib::SharedPtr<rdr::TextureView>	multiScatteringLUT;
};

} // spt::rsc