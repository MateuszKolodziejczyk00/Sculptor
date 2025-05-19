#include "Noise.h"
#include "TileableVolumeNoise.h"


namespace spt::lib::noise
{

Real32 GenerateTileablePerlineNoise3D(math::Vector3f coords, Real32 frequency, Int32 octaveCount)
{
	return TileablePerlinNoise3D(coords.x(), coords.y(), coords.y(), frequency, octaveCount);
}

Real32 GenerateTileableWorleyNoise3D(math::Vector3f coords, Real32 cellCount)
{
	return TileableWorleyNoise3D(coords.x(), coords.y(), coords.z(), cellCount);
}

} // spt::lib::noise
