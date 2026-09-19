#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Utility/NamedType.h"


namespace spt::rsc::ddgi
{


namespace constants
{
constexpr Uint32 maxTexturesPerVolume = 24u;
constexpr Uint32 maxVolumesCount      = 4u;
constexpr Uint32 maxLODLevels         = 4u;

} // constants


struct DDGIVolumeParams
{
	math::Vector3u probesVolumeResolution;
	math::Vector3u relitZoneResolution;

	math::Vector3f probesOriginWorldLocation;

	math::Vector3f probesSpacing;

	math::Vector2u probeIlluminanceDataRes;
	
	math::Vector2u probeHitDistanceDataRes;

	Real32         probeIlluminanceEncodingGamma = 1.f;

	Real32         priority = 1.f;
};


using VolumeIlluminanceTexturesArray = lib::StaticArray<gfx::ConstSRVTexture2D<math::Vector4f>, constants::maxTexturesPerVolume>;
using VolumeHitDistanceTexturesArray = lib::StaticArray<gfx::ConstSRVTexture2D<math::Vector2f>, constants::maxTexturesPerVolume>;


BEGIN_SHADER_STRUCT(DDGIVolumeGPUParams)
	SHADER_STRUCT_FIELD(math::Vector3f, probesOriginWorldLocation) // AABB begin
	SHADER_STRUCT_FIELD(math::Vector3f, probesEndWorldLocation) // AABB end
	
	SHADER_STRUCT_FIELD(math::Vector3f, probesSpacing)
	SHADER_STRUCT_FIELD(math::Vector3f, rcpProbesSpacing)
	SHADER_STRUCT_FIELD(Real32,         maxDistBetweenProbes)
	
	SHADER_STRUCT_FIELD(math::Vector3u, probesVolumeResolution)
	SHADER_STRUCT_FIELD(Bool,           isValid)
	SHADER_STRUCT_FIELD(math::Vector3i, probesWrapCoords)
	
	SHADER_STRUCT_FIELD(math::Vector2u, probesIlluminanceTextureRes)
	SHADER_STRUCT_FIELD(math::Vector2u, probesHitDistanceTextureRes)
	
	SHADER_STRUCT_FIELD(math::Vector2u, probeIlluminanceDataRes)
	SHADER_STRUCT_FIELD(math::Vector2u, probeIlluminanceDataWithBorderRes)
	
	SHADER_STRUCT_FIELD(math::Vector2u, probeHitDistanceDataRes)
	SHADER_STRUCT_FIELD(math::Vector2u, probeHitDistanceDataWithBorderRes)
	
	SHADER_STRUCT_FIELD(math::Vector2f, probesIlluminanceTexturePixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, probesIlluminanceTextureUVDeltaPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2f, probesIlluminanceTextureUVPerProbeNoBorder)
	
	SHADER_STRUCT_FIELD(math::Vector2f, probesHitDistanceTexturePixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, probesHitDistanceUVDeltaPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2f, probesHitDistanceTextureUVPerProbeNoBorder)
	SHADER_STRUCT_FIELD(Real32,         probeIlluminanceEncodingGamma)

	SHADER_STRUCT_FIELD(VolumeIlluminanceTexturesArray,         illuminanceTextures)
	SHADER_STRUCT_FIELD(VolumeHitDistanceTexturesArray,         hitDistanceTextures)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture3D<math::Vector3f>, averageLuminanceTexture)
END_SHADER_STRUCT();


struct DDGIVolumeGPUDefinition
{
	DDGIVolumeGPUParams gpuParams;
};


BEGIN_SHADER_STRUCT(DDGIRelitGPUParams)
	SHADER_STRUCT_FIELD(math::Vector3u, probesToUpdateCoords)
	SHADER_STRUCT_FIELD(Real32,         probeRaysMaxT)
	SHADER_STRUCT_FIELD(math::Vector3u, probesToUpdateCount)
	SHADER_STRUCT_FIELD(Real32,         probeRaysMinT)
	SHADER_STRUCT_FIELD(Uint32,         raysNumPerProbe)
	SHADER_STRUCT_FIELD(Uint32,         probesNumToUpdate)
	SHADER_STRUCT_FIELD(Real32,         rcpRaysNumPerProbe)
	SHADER_STRUCT_FIELD(Real32,         rcpProbesNumToUpdate)
	SHADER_STRUCT_FIELD(Real32,         blendHysteresis)
	SHADER_STRUCT_FIELD(Real32,         luminanceDiffThreshold)
	SHADER_STRUCT_FIELD(math::Vector3f, prevAABBMin)
	SHADER_STRUCT_FIELD(math::Vector3f, prevAABBMax)
END_SHADER_STRUCT();


// Needs additional padding because it's used in array (so size must be multiple of 16)
BEGIN_SHADER_STRUCT(DDGILODDefinition)
	SHADER_STRUCT_FIELD(Uint32, volumeIdx)
	SHADER_STRUCT_FIELD(Uint32, padding0)
	SHADER_STRUCT_FIELD(Uint32, padding1)
	SHADER_STRUCT_FIELD(Uint32, padding2)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGILODsDefinition)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<DDGILODDefinition, constants::maxLODLevels>), lods)
	SHADER_STRUCT_FIELD(Uint32,                                                                       lodsNum)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGIVolumesDefinition)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<DDGIVolumeGPUParams, constants::maxVolumesCount>), volumes)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGIGPUScene)
	SHADER_STRUCT_FIELD(DDGILODsDefinition,    ddgiLODs)
	SHADER_STRUCT_FIELD(DDGIVolumesDefinition, volumesDef)
END_SHADER_STRUCT();


class DDGIGPUVolumeHandle
{
public:

	DDGIGPUVolumeHandle();

	DDGIGPUVolumeHandle(Uint32 index, DDGIVolumeGPUParams& volumeParams, const DDGIVolumeGPUDefinition& volumeGPUDefinition);

	bool IsValid() const;

	void Destroy();

	Uint32 GetVolumeIdx() const;

	const DDGIVolumeGPUParams& GetGPUParams() const;
	DDGIVolumeGPUParams&       GetGPUParamsMutable();

	const Uint32 GetProbesDataTexturesNum() const;

	lib::SharedPtr<rdr::TextureView> GetProbesIlluminanceTexture(Uint32 textureIdx) const;
	lib::SharedPtr<rdr::TextureView> GetProbesHitDistanceTexture(Uint32 textureIdx) const;
	lib::SharedPtr<rdr::TextureView> GetProbesAverageLuminanceTexture() const;

private:

	DDGIVolumeGPUParams* m_volumeParams = nullptr;

	Uint32 m_index = idxNone<Uint32>;
};

} // spt::rsc::ddgi
