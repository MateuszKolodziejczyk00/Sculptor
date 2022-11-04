#include "Engine.h"
#include "Paths.h"

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

	m_cmdLineArgs.Parse(initializationParams.cmdLineArgsNum, initializationParams.cmdLineArgs);

	Paths::Initialize(m_cmdLineArgs);
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

} // spt::engn
