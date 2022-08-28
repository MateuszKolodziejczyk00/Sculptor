#include "Engine.h"

namespace spt::engn
{

void Engine::Initialize(const EngineInitializationParams& initializationParams)
{
	SPT_PROFILE_FUNCTION();

	Engine& engineInstance = GetInstance();
	
	engineInstance.m_cmdLineArgs.Parse(initializationParams.cmdLineArgsNum, initializationParams.cmdLineArgs);

	engineInstance.m_relPathToEngineDirectory = engineInstance.m_cmdLineArgs.GetValue("EngineRelativePath").ToString();
}

const lib::String& Engine::GetRelPathToEngine()
{
	return GetInstance().m_relPathToEngineDirectory;
}

Engine& Engine::GetInstance()
{
	static Engine instance;
	return instance;
}

} // spt::engn
