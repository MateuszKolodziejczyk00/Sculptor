#pragma once

#include "EngineCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Utils/CommandLineArguments.h"
#include "Delegates/MulticastDelegate.h"
#include "EngineTimer.h"


namespace spt::engn
{

struct EngineInitializationParams
{
	EngineInitializationParams()
	{ }

	platf::CmdLineArgs additionalArgs;
};


class ENGINE_CORE_API Engine
{
public:

	static Engine& Get();

	void Initialize(const EngineInitializationParams& initializationParams);

	Real32 BeginFrame();

	using  OnBeginFrameDelegate = lib::MulticastDelegate<void()>;
	OnBeginFrameDelegate& GetOnBeginFrameDelegate();

	const CommandLineArguments& GetCmdLineArgs();

	const EngineTimer& GetEngineTimer() const;
	Real32 GetTime() const;

private:

	Engine() = default;

	EngineInitializationParams m_initParams;

	CommandLineArguments m_cmdLineArgs;

	EngineTimer m_timer;
	OnBeginFrameDelegate m_onBeginFrameDelegate;
};


ENGINE_CORE_API Engine&            GetEngine();
ENGINE_CORE_API const EngineTimer& GetEngineTimer();

} // spt::engn
