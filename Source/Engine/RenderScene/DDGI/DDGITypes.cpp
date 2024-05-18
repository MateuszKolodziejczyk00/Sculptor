#include "DDGITypes.h"


namespace spt::rsc::ddgi
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIGPUVolume =================================================================================

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle() = default;

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle(lib::MTHandle<DDGISceneDS> sceneDS, Uint32 index, const DDGIVolumeGPUDefinition& volumeGPUDefinition)
	: m_ddgiSceneDS(std::move(sceneDS))
	, m_index(index)
{
	SPT_CHECK(m_ddgiSceneDS.IsValid());
	SPT_CHECK(m_index != idxNone<Uint32>);

	DDGIVolumeGPUParams& gpuParams = m_ddgiSceneDS->u_volumesDef.GetMutable().volumes[m_index];

	gpuParams = volumeGPUDefinition.gpuParams;

	m_illuminanceTextureBindingsAllocation      = volumeGPUDefinition.illuminanceTexturesAllocation;
	m_hitDistanceTextureBindingsAllocation      = volumeGPUDefinition.hitDistanceTexturesAllocation;
}

bool DDGIGPUVolumeHandle::IsValid() const
{
	return m_ddgiSceneDS.IsValid() && m_index != idxNone<Uint32>;
}

void DDGIGPUVolumeHandle::Destroy()
{
	if (IsValid())
	{
		DDGIVolumeGPUParams& gpuParams = m_ddgiSceneDS->u_volumesDef.GetMutable().volumes[m_index];

		m_ddgiSceneDS->u_probesTextures2D.UnbindAndDeallocateTexturesBlock(m_illuminanceTextureBindingsAllocation);
		m_ddgiSceneDS->u_probesTextures2D.UnbindAndDeallocateTexturesBlock(m_hitDistanceTextureBindingsAllocation);

		m_ddgiSceneDS->u_probesTextures3D.UnbindTexture(gpuParams.averageLuminanceTextureIdx);

		gpuParams = DDGIVolumeGPUParams();
		m_index = idxNone<Uint32>;

		m_illuminanceTextureBindingsAllocation.Reset();
		m_hitDistanceTextureBindingsAllocation.Reset();
	}
}

Uint32 DDGIGPUVolumeHandle::GetVolumeIdx() const
{
	return m_index;
}

const DDGIVolumeGPUParams& DDGIGPUVolumeHandle::GetGPUParams() const
{
	SPT_CHECK(IsValid());
	const DDGIVolumesDefinition& volumesDef = m_ddgiSceneDS->u_volumesDef.Get();
	return volumesDef.volumes[m_index];
}

DDGIVolumeGPUParams& DDGIGPUVolumeHandle::GetGPUParamsMutable()
{
	SPT_CHECK(IsValid());
	DDGIVolumesDefinition& volumesDef = m_ddgiSceneDS->u_volumesDef.GetMutable();
	return volumesDef.volumes[m_index];
}

const Uint32 DDGIGPUVolumeHandle::GetProbesDataTexturesNum() const
{
	return IsValid() ? GetGPUParams().probesVolumeResolution.z() : 0u;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesIlluminanceTexture(Uint32 textureIdx) const
{
	return IsValid() ? m_ddgiSceneDS->u_probesTextures2D.GetBoundTexture<lib::SharedPtr<rdr::TextureView>>(m_illuminanceTextureBindingsAllocation, textureIdx) : nullptr;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesHitDistanceTexture(Uint32 textureIdx) const
{
	return IsValid() ? m_ddgiSceneDS->u_probesTextures2D.GetBoundTexture<lib::SharedPtr<rdr::TextureView>>(m_hitDistanceTextureBindingsAllocation, textureIdx) : nullptr;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesAverageLuminanceTexture() const
{
	return IsValid() ? m_ddgiSceneDS->u_probesTextures3D.GetBoundTexture<lib::SharedPtr<rdr::TextureView>>(GetGPUParams().averageLuminanceTextureIdx) : nullptr;
}

} // spt::rsc::ddgi
