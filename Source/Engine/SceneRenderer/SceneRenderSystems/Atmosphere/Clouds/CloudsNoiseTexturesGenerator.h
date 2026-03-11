#pragma once

#include "SculptorCoreTypes.h"
#include "Types/Texture.h"


namespace spt::rsc::clouds
{

struct CloudsNoiseData
{
	math::Vector3u resolution          = {};
	rhi::EFragmentFormat format        = {};
	lib::DynamicArray<Byte> linearData = {};
};

CloudsNoiseData ComputeBaseShapeNoiseTextureWorley();

CloudsNoiseData ComputeDetailShapeNoiseTextureWorley();

} // spt::rsc::clouds
