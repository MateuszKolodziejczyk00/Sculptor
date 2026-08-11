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

CloudsNoiseData Compute2DPerlinWorley();

CloudsNoiseData Compute2DPerlin();

} // spt::rsc::clouds
