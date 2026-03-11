#include "DDGITypes.h"


namespace spt::rsc::ddgi
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DDGIGPUVolume =================================================================================

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle() = default;

DDGIGPUVolumeHandle::DDGIGPUVolumeHandle(lib::MTHandle<DDGISceneDS> sceneDS, Uint32 index, DDGIVolumeGPUParams& volumeParams, const DDGIVolumeGPUDefinition& volumeGPUDefinition)
	: m_volumeParams(&volumeParams)
	, m_index(index)
	, m_ddgiSceneDS(std::move(sceneDS))
{
	SPT_CHECK(m_ddgiSceneDS.IsValid());
	SPT_CHECK(m_index != idxNone<Uint32>);

	*m_volumeParams = volumeGPUDefinition.gpuParams;

	m_illuminanceTextureBindingsAllocation = volumeGPUDefinition.illuminanceTexturesAllocation;
	m_hitDistanceTextureBindingsAllocation = volumeGPUDefinition.hitDistanceTexturesAllocation;
}

bool DDGIGPUVolumeHandle::IsValid() const
{
	return m_ddgiSceneDS.IsValid() && m_index != idxNone<Uint32>;
}

void DDGIGPUVolumeHandle::Destroy()
{
	if (IsValid())
	{
		m_ddgiSceneDS->u_probesTextures2D.UnbindAndDeallocateTexturesBlock(m_illuminanceTextureBindingsAllocation);
		m_ddgiSceneDS->u_probesTextures2D.UnbindAndDeallocateTexturesBlock(m_hitDistanceTextureBindingsAllocation);

		m_ddgiSceneDS->u_probesTextures3D.UnbindTexture(m_volumeParams->averageLuminanceTextureIdx);

		*m_volumeParams = DDGIVolumeGPUParams();
		m_index = idxNone<Uint32>;
		m_volumeParams = nullptr;

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
