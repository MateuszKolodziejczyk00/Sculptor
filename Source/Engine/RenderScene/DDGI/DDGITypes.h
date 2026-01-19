#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Utility/NamedType.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/StructuredCPUToGPUBufferBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/ArrayOfTextureBlocksBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


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

	SHADER_STRUCT_FIELD(Uint32,         illuminanceTextureIdx)
	SHADER_STRUCT_FIELD(Uint32,         hitDistanceTextureIdx)
	SHADER_STRUCT_FIELD(Uint32,         averageLuminanceTextureIdx)
END_SHADER_STRUCT();


struct DDGIVolumeGPUDefinition
{
	DDGIVolumeGPUParams gpuParams;

	gfx::TexturesBindingsAllocationHandle illuminanceTexturesAllocation;
	gfx::TexturesBindingsAllocationHandle hitDistanceTexturesAllocation;
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


namespace EDDGIDebugMode
{
enum Type
{
	None,
	Illuminance,
	HitDistance,
	DebugRays,

	NUM
};
} // EDDDGIProbesDebugMode


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


DS_BEGIN(DDGISceneDS, rg::RGDescriptorSetState<DDGISceneDS>)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTexture2DBlocksBinding<math::Vector4f, constants::maxVolumesCount * constants::maxTexturesPerVolume * 2, true>), u_probesTextures2D)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures3DBinding<constants::maxVolumesCount, true>),                                                            u_probesTextures3D)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),                                                            u_probesDataSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<DDGILODsDefinition>),                                                                    u_ddgiLODs)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBindingStaticOffset<DDGIVolumesDefinition>),                                                                 u_volumesDef)
DS_END();


class DDGIGPUVolumeHandle
{
public:

	DDGIGPUVolumeHandle();

	DDGIGPUVolumeHandle(lib::MTHandle<DDGISceneDS> sceneDS, Uint32 index, DDGIVolumeGPUParams& volumeParams, const DDGIVolumeGPUDefinition& volumeGPUDefinition);

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

	lib::MTHandle<DDGISceneDS> m_ddgiSceneDS;

	gfx::TexturesBindingsAllocationHandle m_illuminanceTextureBindingsAllocation;
	gfx::TexturesBindingsAllocationHandle m_hitDistanceTextureBindingsAllocation;
};

} // spt::rsc::ddgi
