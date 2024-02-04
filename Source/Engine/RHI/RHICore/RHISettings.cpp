#include "RHICore/RHISettings.h"
#include "Engine.h"

namespace spt::rhi
{

RHISettings::RHISettings()
{ }

void RHISettings::Initialize()
{
	SPT_PROFILER_FUNCTION();

	const engn::CommandLineArguments& cmdLineArgs = engn::Engine::Get().GetCmdLineArgs();

	enableValidation       = cmdLineArgs.Contains(enableValidationCmdArgName);
	enableStrictValidation = cmdLineArgs.Contains(enableStrictValidationCmdArgName);
	enableValidation       |= enableStrictValidation;

	enableRayTracing = cmdLineArgs.Contains(enableRayTracingCmdArgName);

	enableGPUCrashDumps = cmdLineArgs.Contains(enableGPUCrashDumpsCmdArgName);
	
	enablePersistentDebugNames = cmdLineArgs.Contains(enablePersistentDebugNamesCmdArgName);
}

} // spt::rhi
