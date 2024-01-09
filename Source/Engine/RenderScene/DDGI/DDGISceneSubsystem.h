#pragma once

#include "RenderSceneSubsystem.h"
#include "DDGITypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "RenderSceneRegistry.h"
#include "DDGIScene.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc::ddgi
{

class DDGIVolume;


struct DDGIConfig
{
	Uint32 localRelitRaysNumPerProbe  = 1024u;
	Uint32 globalRelitRaysPerProbe    = 400u;

	Real32 globalRelitHysteresis = 0.97f;
	Real32 localRelitHysteresis  = 0.98f;

	math::Vector3u localRelitProbeGridSize = math::Vector3u(6, 6, 6);

	Uint32 relitVolumesBudget = 3u;
	
	math::Vector2u probeIlluminanceDataRes = math::Vector2u::Constant(6u);
	math::Vector2u probeHitDistanceDataRes = math::Vector2u::Constant(16u);

	// Lod 0

	math::Vector3u lod0VolumeResolution = math::Vector3u::Constant(12u);
	math::Vector3f lod0ProbesSpacing    = math::Vector3f::Constant(1.5f);

	// Lod 1

	math::Vector3u lod1VolumeResolution = math::Vector3u(12u, 12u, 8u);
	math::Vector3f lod1ProbesSpacing    = math::Vector3f::Constant(3.f);
	Real32         lod1VolumesMinHeight = -2.f;
};


class RENDER_SCENE_API DDGISceneSubsystem : public RenderSceneSubsystem
{
protected:

	using Super = RenderSceneSubsystem;

public:

	explicit DDGISceneSubsystem(RenderScene& owningScene);

	void UpdateDDGIScene(const SceneView& mainView);

	void SetDebugMode(EDDGIDebugMode::Type mode);
	EDDGIDebugMode::Type GetDebugMode() const;

	const lib::MTHandle<DDGISceneDS>& GetDDGISceneDS() const;

	Bool IsDDGIEnabled() const;

	const DDGIScene& GetDDGIScene() const;
	DDGIScene& GetDDGIScene();

	const DDGIConfig& GetConfig() const;

private:

	DDGIConfig m_config;

	EDDGIDebugMode::Type m_debugMode;

	DDGIScene m_ddgiScene;
};

} // spt::rsc::ddgi