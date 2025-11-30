#include "DDGISceneSubsystem.h"
#include "Types/Texture.h"
#include "ResourcesManager.h"
#include "SceneRenderer/Parameters/SceneRendererParams.h"
#include "EngineFrame.h"
#include "RenderScene.h"
#include "Lights/LightTypes.h"
#include "ConfigUtils.h"
#include "DDGIVolume.h"

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
	engn::ConfigUtils::LoadConfigData(m_config, "DDGIConfig.json");

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
