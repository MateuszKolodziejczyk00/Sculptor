#include "Engine.h"
#include "Paths.h"
#include "Plugins/PluginsManager.h"

namespace spt::engn
{

Engine* g_engineInstance = nullptr;

Engine& Engine::Get()
{
	SPT_CHECK(g_engineInstance != nullptr);
	return *g_engineInstance;
}

void Engine::Initialize(const EngineInitializationParams& initializationParams)
{
	SPT_PROFILER_FUNCTION();

	lib::HashedStringDB::Initialize();

	SPT_CHECK(g_engineInstance == nullptr);
	g_engineInstance = new Engine();

	platf::CmdLineArgs cmdLineArgs = platf::Platform::GetCommandLineArguments();
	cmdLineArgs.insert(cmdLineArgs.end(), initializationParams.additionalArgs.cbegin(), initializationParams.additionalArgs.cend());

	g_engineInstance->m_cmdLineArgs.Parse(cmdLineArgs);

	Paths::Initialize(g_engineInstance->m_cmdLineArgs);

	g_engineInstance->m_modulesManager.Initialize();

	PluginsManager::GetInstance().PostEngineInit();
}

void Engine::InitializeModule(Engine& engineInstance)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(g_engineInstance == nullptr);
	g_engineInstance = &engineInstance;
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
