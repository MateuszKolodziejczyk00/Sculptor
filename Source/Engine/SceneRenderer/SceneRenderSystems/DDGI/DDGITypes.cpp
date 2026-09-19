#include "DDGITypes.h"


namespace spt::rsc::ddgi
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIGPUVolume =================================================================================

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle() = default;

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle(Uint32 index, DDGIVolumeGPUParams& volumeParams, const DDGIVolumeGPUDefinition& volumeGPUDefinition)
	: m_volumeParams(&volumeParams)
	, m_index(index)
{
	SPT_CHECK(m_index != idxNone<Uint32>);

	*m_volumeParams = volumeGPUDefinition.gpuParams;
}

bool DDGIGPUVolumeHandle::IsValid() const
{
	return m_volumeParams != nullptr && m_index != idxNone<Uint32>;
}

void DDGIGPUVolumeHandle::Destroy()
{
	if (IsValid())
	{
		*m_volumeParams = DDGIVolumeGPUParams();
		m_index = idxNone<Uint32>;
		m_volumeParams = nullptr;
	}
}

Uint32 DDGIGPUVolumeHandle::GetVolumeIdx() const
{
	return m_index;
}

const DDGIVolumeGPUParams& DDGIGPUVolumeHandle::GetGPUParams() const
{
	SPT_CHECK(IsValid());
	return *m_volumeParams;
}

DDGIVolumeGPUParams& DDGIGPUVolumeHandle::GetGPUParamsMutable()
{
	SPT_CHECK(IsValid());
	return *m_volumeParams;
}

const Uint32 DDGIGPUVolumeHandle::GetProbesDataTexturesNum() const
{
	return IsValid() ? GetGPUParams().probesVolumeResolution.z() : 0u;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesIlluminanceTexture(Uint32 textureIdx) const
{
	return IsValid() ? m_volumeParams->illuminanceTextures[textureIdx].GetTextureView() : nullptr;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesHitDistanceTexture(Uint32 textureIdx) const
{
	return IsValid() ? m_volumeParams->hitDistanceTextures[textureIdx].GetTextureView() : nullptr;
}

lib::SharedPtr<rdr::TextureView> DDGIGPUVolumeHandle::GetProbesAverageLuminanceTexture() const
{
	return IsValid() ? m_volumeParams->averageLuminanceTexture.GetTextureView() : nullptr;
}

} // spt::rsc::ddgi
