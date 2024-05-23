#include "DDGISceneSubsystem.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "EngineFrame.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "YAMLSerializerHelper.h"
#include "ConfigUtils.h"
#include "DDGIVolume.h"

namespace spt::srl
{

template<>
struct TypeSerializer<rsc::ddgi::DDGILODConfig>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("VolumeResolution",    data.volumeResolution);
		serializer.Serialize("RelitZoneResolution", data.relitZoneResolution);
		serializer.Serialize("ProbesSpacing",       data.probesSpacing);
		serializer.Serialize("RelitPriority",       data.relitPriority);
		serializer.Serialize("ForwardAlignment",    data.forwardAlignment);
		serializer.Serialize("HeightAlignment",     data.heightAlignment);
	}
};

template<>
struct TypeSerializer<rsc::ddgi::DDGIConfig>
{
	template<typename Serializer, typename Param>
	static void Serialize(SerializerWrapper<Serializer>& serializer, Param& data)
	{
		serializer.Serialize("LocalRelitRaysNumPerProbe", data.localRelitRaysNumPerProbe);
		serializer.Serialize("GlobalRelitRaysPerProbe", data.globalRelitRaysPerProbe);

		serializer.Serialize("GlobalRelitHysteresis", data.defaultGlobalRelitHysteresis);
		serializer.Serialize("LocalRelitHysteresis", data.defaultLocalRelitHysteresis);

		serializer.Serialize("MinHysteresis", data.minHysteresis);
		serializer.Serialize("MaxHysteresis", data.maxHysteresis);

		serializer.Serialize("LocalRelitProbeGridSize", data.localRelitProbeGridSize);

		serializer.Serialize("RelitVolumesBudget", data.relitVolumesBudget);
		serializer.Serialize("ProbeIlluminanceDataRes", data.probeIlluminanceDataRes);
		serializer.Serialize("ProbeHitDistanceDataRes", data.probeHitDistanceDataRes);

		serializer.Serialize("LodsConfigs", data.lodsConfigs);
	}
};

} // spt::srl

SPT_YAML_SERIALIZATION_TEMPLATES(spt::rsc::ddgi::DDGILODConfig)
SPT_YAML_SERIALIZATION_TEMPLATES(spt::rsc::ddgi::DDGIConfig)

namespace spt::rsc::ddgi
{

namespace parameters
{

RendererBoolParameter ddgiEnabled("Enable DDGI", {"DDGI"}, true);

} // parameters

DDGISceneSubsystem::DDGISceneSubsystem(RenderScene& owningScene)
	: Super(owningScene)
	, m_debugMode(EDDGIDebugMode::None)
	, m_ddgiScene(owningScene)
{
	engn::ConfigUtils::LoadConfigData(m_config, "DDGIConfig.yaml");

	m_ddgiScene.Initialize(m_config);
}

void DDGISceneSubsystem::UpdateDDGIScene(const SceneView& mainView)
{
	m_ddgiScene.Update(mainView);
}

void DDGISceneSubsystem::SetDebugMode(EDDGIDebugMode::Type mode)
{
	m_debugMode = mode;
}

EDDGIDebugMode::Type DDGISceneSubsystem::GetDebugMode() const
{
	return m_debugMode;
}

const lib::MTHandle<DDGISceneDS>& DDGISceneSubsystem::GetDDGISceneDS() const
{
	return GetDDGIScene().GetDDGIDS();
}

Bool DDGISceneSubsystem::IsDDGIEnabled() const
{
	return parameters::ddgiEnabled;
}

const DDGIScene& DDGISceneSubsystem::GetDDGIScene() const
{
	return m_ddgiScene;
}

DDGIScene& DDGISceneSubsystem::GetDDGIScene()
{
	return m_ddgiScene;
}

const DDGIConfig& DDGISceneSubsystem::GetConfig() const
{
	return m_config;
}

} // spt::rsc::ddgi
