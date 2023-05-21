#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{
class Texture;
} // spt::rdr


namespace spt::rsc
{

struct CameraLensSettingsComponent
{
	CameraLensSettingsComponent()
		: lensDirtIntensity(math::Vector3f::Constant(0.35f))
		, lensDirtThreshold(math::Vector3f::Constant(0.1f))
	{ }

	lib::SharedPtr<rdr::TextureView> lensDirtTexture;
	math::Vector3f lensDirtIntensity;
	math::Vector3f lensDirtThreshold;
};

} // spt::rsc
