#pragma once

#include "RenderSceneSubsystem.h"
#include "DDGITypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RenderSceneRegistry.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

DS_BEGIN(DDGIDS, rg::RGDescriptorSetState<DDGIDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DDGIGPUParams>),					u_ddgiParams)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_probesIlluminanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector2f>),								u_probesHitDistanceTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_probesDataSampler)
DS_END();


struct DDGIConfig
{
	math::Vector3u probesVolumeResolution = math::Vector3u::Constant(12u);
	math::Vector3u localProbesUpdateResolution = math::Vector3u::Constant(6u);
	
	math::Vector3f probesSpacing = math::Vector3f(1.f, 2.f, 1.f);

	Uint32 localUpdateRaysPerProbe = 1024u;
	Uint32 globalUpdateRaysPerProbe = 380u;
};


class RENDER_SCENE_API DDGISceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit DDGISceneSubsystem(RenderScene& owningScene);
	~DDGISceneSubsystem();

	DDGIUpdateProbesGPUParams CreateUpdateProbesParams() const;

	const lib::SharedPtr<rdr::TextureView>& GetProbesIlluminanceTexture() const;
	
	const lib::SharedPtr<rdr::TextureView>& GetProbesHitDistanceTexture() const;
	
	const DDGIGPUParams& GetDDGIParams() const;

	void SetProbesDebugMode(EDDDGIProbesDebugMode::Type mode);
	EDDDGIProbesDebugMode::Type GetProbesDebugMode() const;

	const lib::SharedPtr<DDGIDS>& GetDDGIDS() const;

	Bool IsDDGIEnabled() const;

	Bool RequiresClearingData() const;
	void PostClearingData();

	void PostUpdateProbes();

	// Settings ==================================================================

	Uint32 GetRaysNumPerProbe() const;

	Uint32 GetProbesNum() const;

	math::Vector3u GetProbesVolumeResolution() const;

	math::Vector2u GetProbeIlluminanceDataRes() const;
	math::Vector2u GetProbeIlluminanceWithBorderDataRes() const;
	
	math::Vector2u GetProbeDistancesDataRes() const;
	math::Vector2u GetProbeDistancesDataWithBorderRes() const;

private:

	void InitializeDDGIParameters();

	void InitializeTextures();

	void OnDirectionalLightUpdated(RenderSceneRegistry& registry, RenderSceneEntity entity);

	lib::SharedPtr<rdr::TextureView> m_probesIlluminanceTextureView;
	lib::SharedPtr<rdr::TextureView> m_probesHitDistanceTextureView;

	DDGIGPUParams m_ddgiParams;

	DDGIConfig m_config;

	EDDDGIProbesDebugMode::Type m_probesDebugMode;

	lib::SharedPtr<DDGIDS> m_ddgiDS;

	Bool m_requiresClearingData;

	Bool m_wantsGlobalUpdate;
};

} // spt::rsc