#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(SSTracerData)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, linearDepth)
	SHADER_STRUCT_FIELD(math::Vector2u,            resolution)
	SHADER_STRUCT_FIELD(Uint32,                    stepsNum)
END_SHADER_STRUCT()


inline SSTracerData CreateScreenSpaceTracerData(rg::RGTextureViewHandle linearDepth, Uint32 stepsNum)
{
	SSTracerData data;
	data.linearDepth = linearDepth;
	data.resolution  = linearDepth->GetResolution2D();
	data.stepsNum    = stepsNum;

	return data;
}

} // spt::rsc
