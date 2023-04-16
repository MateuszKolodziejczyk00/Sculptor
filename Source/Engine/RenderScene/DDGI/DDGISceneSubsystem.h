#pragma once

#include "RenderSceneSubsystem.h"
#include "DDGITypes.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

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

	// Settings ==================================================================

	EDDDGIProbesDebugType GetProbesDebugType() const;

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
};

} // spt::rsc