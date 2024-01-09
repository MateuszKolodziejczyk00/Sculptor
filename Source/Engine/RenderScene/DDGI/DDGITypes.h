#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "Utility/NamedType.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/StructuredCPUToGPUBufferBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rsc::ddgi
{

struct DDGIVolumeParams
{
	math::Vector3u     probesVolumeResolution;

	math::Vector3f     probesOriginWorldLocation;

	math::Vector3f     probesSpacing;

	math::Vector2u     probeIlluminanceDataRes;
	
	math::Vector2u     probeHitDistanceDataRes;

	Real32             probeIlluminanceEncodingGamma = 1.f;

	Real32             priority = 1.f;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, DDGIVolumeGPUParams)
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
END_SHADER_STRUCT();


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
	SHADER_STRUCT_FIELD(Real32,         illuminanceDiffThreshold)
	SHADER_STRUCT_FIELD(Real32,         luminanceDiffThreshold)
	SHADER_STRUCT_FIELD(math::Matrix4f, raysRotation)
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


constexpr Uint32 g_maxVolumesCount = 32;


BEGIN_SHADER_STRUCT(DDGILOD0Definition)
	SHADER_STRUCT_FIELD(Uint32, lod0VolumeIdx)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGILOD1Definition)
	SHADER_STRUCT_FIELD(math::Vector3f, volumesAABBMin)
	SHADER_STRUCT_FIELD(math::Vector3f, volumesAABBMax)
	SHADER_STRUCT_FIELD(math::Vector3f, rcpSingleVolumeSize)
	SHADER_STRUCT_FIELD(Uint32,         padding)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<math::Vector4u, 9>), volumeIndices) // use uint4 because in hlsl array elements are aligned to 16 bytes
END_SHADER_STRUCT();


DS_BEGIN(DDGISceneDS, rg::RGDescriptorSetState<DDGISceneDS>)
	DS_BINDING(BINDING_TYPE(gfx::StructuredCPUToGPUBufferBinding<DDGIVolumeGPUParams, g_maxVolumesCount, rhi::EMemoryUsage::GPUOnly>), u_ddgiVolumes)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<g_maxVolumesCount * 2, true>),                                      u_probesTextures)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),                                 u_probesDataSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DDGILOD0Definition>),                                                     u_ddgiLOD0)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DDGILOD1Definition>),                                                     u_ddgiLOD1)
DS_END();


class DDGIGPUVolumeHandle
{
public:

	DDGIGPUVolumeHandle();

	DDGIGPUVolumeHandle(lib::MTHandle<DDGISceneDS> sceneDS, Uint32 index);

	bool IsValid() const;

	void Destroy();

	Uint32 GetVolumeIdx() const;

	const DDGIVolumeGPUParams& GetGPUParams() const;
	DDGIVolumeGPUParams&       GetGPUParamsMutable();

	lib::SharedPtr<rdr::TextureView> GetProbesIlluminanceTexture() const;
	lib::SharedPtr<rdr::TextureView> GetProbesHitDistanceTexture() const;

private:

	Uint32 m_index = idxNone<Uint32>;

	lib::MTHandle<DDGISceneDS> m_ddgiSceneDS;
};

} // spt::rsc::ddgi
