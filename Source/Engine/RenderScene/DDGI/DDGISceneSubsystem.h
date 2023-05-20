#pragma once

#include "RenderSceneSubsystem.h"
#include "DDGITypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

DS_BEGIN(DDGIDS, rg::RGDescriptorSetState<DDGIDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DDGIGPUParams>),					u_ddgiParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_probesIrradianceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_probesHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_probesDataSampler)
DS_END();


class RENDER_SCENE_API DDGISceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit DDGISceneSubsystem(RenderScene& owningScene);

	DDGIUpdateProbesGPUParams CreateUpdateProbesParams() const;

	const lib::SharedPtr<rdr::TextureView>& GetProbesIrradianceTexture() const;
	
	const lib::SharedPtr<rdr::TextureView>& GetProbesHitDistanceTexture() const;
	
	const DDGIGPUParams& GetDDGIParams() const;

	void SetProbesDebugMode(EDDDGIProbesDebugMode::Type mode);
	EDDDGIProbesDebugMode::Type GetProbesDebugMode() const;

	const lib::SharedPtr<DDGIDS>& GetDDGIDS() const;

	Bool IsDDGIEnabled() const;

	Bool RequiresClearingData() const;
	void PostClearingData();

	// Settings ==================================================================

	Uint32 GetRaysNumPerProbe() const;

	Uint32 GetProbesNum() const;

	math::Vector3u GetProbesVolumeResolution() const;

	math::Vector2u GetProbeIrradianceDataRes() const;
	math::Vector2u GetProbeIrradianceWithBorderDataRes() const;
	
	math::Vector2u GetProbeDistancesDataRes() const;
	math::Vector2u GetProbeDistancesDataWithBorderRes() const;

private:

	void InitializeDDGIParameters();

	void InitializeTextures();

	lib::SharedPtr<rdr::TextureView> m_probesIrradianceTextureView;
	lib::SharedPtr<rdr::TextureView> m_probesHitDistanceTextureView;

	DDGIGPUParams m_ddgiParams;

	EDDDGIProbesDebugMode::Type m_probesDebugMode;

	lib::SharedPtr<DDGIDS> m_ddgiDS;

	math::Vector3u m_probesUpdatedPerFrame;

	Bool m_requiresClearingData;
};

} // spt::rsc