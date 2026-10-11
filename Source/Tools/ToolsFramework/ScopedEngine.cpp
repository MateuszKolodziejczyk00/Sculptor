#include "ScopedEngine.h"
#include "Engine.h"
#include "GPUApi.h"
#include "JobSystem.h"
#include "Pipelines/PSOsLibrary.h"
#include "GlobalShaderResources/GlobalShaderResources.h"


namespace spt::tools
{

ScopedEngine::ScopedEngine()
{
	SPT_PROFILER_FUNCTION();

	const lib::Path executableDirectory = lib::Path(platf::Platform::GetExecutablePath()).parent_path();
	const lib::Path enginePath          = std::filesystem::weakly_canonical(executableDirectory / "../../");

	const lib::String enginePathArg = "-EngineRelativePath=" + enginePath.generic_string();

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.additionalArgs.emplace_back(enginePathArg);
	engn::Engine::Initialize(engineInitializationParams);

	// Nothing else references this translation unit of static Graphics library, so without it linker drops plugin
	gfx::GlobalShaderResourcesManager::Get();

	const Uint32 threadsNum = std::max(std::thread::hardware_concurrency(), 2u);

	js::JobSystemInitializationParams jobSystemParams;
	jobSystemParams.workerThreadsNum = static_cast<SizeType>(threadsNum - 1u);
	js::JobSystem::Initialize(jobSystemParams);

	rdr::GPUApi::Initialize();

	rdr::PSOsLibrary::GetInstance().PrecachePSOs(rdr::PSOPrecacheParams{});
}

ScopedEngine::~ScopedEngine()
{
	SPT_PROFILER_FUNCTION();

	rdr::GPUApi::Uninitialize();

	js::JobSystem::Shutdown();
}

} // spt::tools
