#include "Engine.h"
#include "Paths.h"
#include "Plugins/PluginsManager.h"

namespace spt::engn
{

Engine& Engine::Get()
{
	static Engine instance;
	return instance;
}

void Engine::Initialize(const EngineInitializationParams& initializationParams)
{
	SPT_PROFILER_FUNCTION();

	platf::CmdLineArgs cmdLineArgs = platf::Platform::GetCommandLineArguments();
	cmdLineArgs.insert(cmdLineArgs.end(), initializationParams.additionalArgs.cbegin(), initializationParams.additionalArgs.cend());

	m_cmdLineArgs.Parse(cmdLineArgs);

	Paths::Initialize(m_cmdLineArgs);

	m_modulesManager.Initialize();

	PluginsManager::GetInstance().PostEngineInit();
}

Real32 Engine::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	const float deltaTime = m_timer.Tick();
	m_onBeginFrameDelegate.Broadcast();

	return deltaTime;
}

Engine::OnBeginFrameDelegate& Engine::GetOnBeginFrameDelegate()
{
	return m_onBeginFrameDelegate;
}

const CommandLineArguments& Engine::GetCmdLineArgs()
{
	return m_cmdLineArgs;
}

const EngineTimer& Engine::GetEngineTimer() const
{
	return m_timer;
}

Real32 Engine::GetTime() const
{
	return m_timer.GetTime();
}

Engine& GetEngine()
{
	return Engine::Get();
}

const EngineTimer& GetEngineTimer()
{
	return Engine::Get().GetEngineTimer();
}

} // spt::engn
