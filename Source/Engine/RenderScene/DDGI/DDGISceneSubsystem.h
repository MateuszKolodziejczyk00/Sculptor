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


struct DDGILODConfig
{
	math::Vector3u volumeResolution    = math::Vector3u::Constant(12u);
	math::Vector3u relitZoneResolution = math::Vector3u::Constant(12u);
	math::Vector3f probesSpacing       = math::Vector3f::Constant(1.f);

	Real32 relitPriority = 1.f;

	Real32 forwardAlignment = 0.5f;
	Real32 heightAlignment  = 0.2f;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("VolumeResolution", volumeResolution);
		serializer.Serialize("RelitZoneResolution", relitZoneResolution);
		serializer.Serialize("ProbesSpacing", probesSpacing);
		serializer.Serialize("RelitPriority", relitPriority);
		serializer.Serialize("ForwardAlignment", forwardAlignment);
		serializer.Serialize("HeightAlignment", heightAlignment);
	}
};


struct DDGIConfig
{
	Uint32 relitRaysPerProbe    = 400u;

	Real32 defaultRelitHysteresis = 0.97f;

	Real32 minHysteresis         = 0.3f;
	Real32 maxHysteresis         = 0.995f;

	Uint32 relitVolumesBudget = 3u;
	
	math::Vector2u probeIlluminanceDataRes = math::Vector2u::Constant(6u);
	math::Vector2u probeHitDistanceDataRes = math::Vector2u::Constant(16u);

	lib::DynamicArray<DDGILODConfig> lodsConfigs = { DDGILODConfig{} };

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("RelitRaysPerProbe", relitRaysPerProbe);
		serializer.Serialize("DefaultRelitHysteresis", defaultRelitHysteresis);
		serializer.Serialize("MinHysteresis", minHysteresis);
		serializer.Serialize("MaxHysteresis", maxHysteresis);
		serializer.Serialize("RelitVolumesBudget", relitVolumesBudget);
		serializer.Serialize("ProbeIlluminanceDataRes", probeIlluminanceDataRes);
		serializer.Serialize("ProbeHitDistanceDataRes", probeHitDistanceDataRes);
		serializer.Serialize("LODsConfigs", lodsConfigs);
	}
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