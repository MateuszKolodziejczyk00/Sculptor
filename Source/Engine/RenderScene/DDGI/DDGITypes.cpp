#include "DDGITypes.h"


namespace spt::rsc::ddgi
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIGPUVolume =================================================================================

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle() = default;

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle(lib::MTHandle<DDGISceneDS> sceneDS, Uint32 index)
	: m_ddgiSceneDS(std::move(sceneDS))
	, m_index(index)
{ }

bool DDGIGPUVolumeHandle::IsValid() const
{
	return m_ddgiSceneDS.IsValid() && m_index != idxNone<Uint32>;
}

void DDGIGPUVolumeHandle::Destroy()
{
	if (IsValid())
	{
		DDGIVolumeGPUParams& gpuParams = m_ddgiSceneDS->u_volumesDef.GetMutable().volumes[m_index];

		m_ddgiSceneDS->u_probesTextures.UnbindTexture(gpuParams.illuminanceTextureIdx);
		m_ddgiSceneDS->u_probesTextures.UnbindTexture(gpuParams.hitDistanceTextureIdx);

		gpuParams = DDGIVolumeGPUParams();
		m_index = idxNone<Uint32>;
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

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesIlluminanceTexture() const
{
	return IsValid() ? m_ddgiSceneDS->u_probesTextures.GetBoundTexture<lib::SharedPtr<rdr::TextureView>>(GetGPUParams().illuminanceTextureIdx) : nullptr;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesHitDistanceTexture() const
{
	return IsValid() ? m_ddgiSceneDS->u_probesTextures.GetBoundTexture<lib::SharedPtr<rdr::TextureView>>(GetGPUParams().hitDistanceTextureIdx) : nullptr;
}

} // spt::rsc::ddgi
