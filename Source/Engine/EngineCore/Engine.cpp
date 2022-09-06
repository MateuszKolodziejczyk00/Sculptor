#include "Engine.h"

namespace spt::engn
{

void Engine::Initialize(const EngineInitializationParams& initializationParams)
{
	SPT_PROFILER_FUNCTION();

	Engine& engineInstance = GetInstance();
	
	engineInstance.m_cmdLineArgs.Parse(initializationParams.cmdLineArgsNum, initializationParams.cmdLineArgs);

	Paths::Initialize(engineInstance.m_cmdLineArgs);
}

Engine& Engine::GetInstance()
{
	static Engine instance;
	return instance;
}

} // spt::engn
