#pragma once

#include "SculptorCoreTypes.h"
#include "SculptorLibMacros.h"


namespace spt::lib
{

namespace noise
{

SCULPTOR_LIB_API Real32 GenerateTileablePerlineNoise3D(math::Vector3f coords, Real32 frequency, Int32 octaveCount);

SCULPTOR_LIB_API Real32 GenerateTileableWorleyNoise3D(math::Vector3f coords, Real32 cellCount);

} // noise

} // spt::lib