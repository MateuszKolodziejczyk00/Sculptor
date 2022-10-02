#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"

namespace spt::rhi
{

struct TextureCopyRange
{
public:

	TextureCopyRange()
		: aspect(ETextureAspect::None)
		, mipLevel(0)
		, baseArrayLayer(0)
		, arrayLayersNum(constants::allRemainingArrayLayers)
		, offset(0, 0, 0)
	{ }

	explicit TextureCopyRange(ETextureAspect inAspect, Uint32 inMipLevel = 0, Uint32 inBaseArrayLevel = 0, Uint32 inArrayLayersNum = constants::allRemainingArrayLayers)
		: aspect(inAspect)
		, mipLevel(inMipLevel)
		, baseArrayLayer(inBaseArrayLevel)
		, arrayLayersNum(inArrayLayersNum)
		, offset(math::Vector3i(0, 0, 0))
	{ }

	ETextureAspect	aspect;
	Uint32			mipLevel;
	Uint32			baseArrayLayer;
	Uint32			arrayLayersNum;
	math::Vector3i	offset;
};

} // spt::rhi