#include "ScopedEngine.h"
#include "Engine.h"
#include "GPUApi.h"
#include "JobSystem.h"
#include "ProfilerCore.h"
#include "Pipelines/PSOsLibrary.h"
#include "GlobalShaderResources/GlobalShaderResources.h"


namespace spt::tools
{

ScopedEngine::ScopedEngine(const ScopedEngineParams& params /*= ScopedEngineParams{}*/)
	: m_params(params)
{
	SPT_PROFILER_FUNCTION();

	if (m_params.initializeModules)
	{
		prf::ProfilerCore::GetInstance().Initialize();
	}

	const lib::Path executableDirectory = lib::Path(platf::Platform::GetExecutablePath()).parent_path();
	const lib::Path enginePath          = std::filesystem::weakly_canonical(executableDirectory / "../../");

	const lib::String enginePathArg = "-EngineRelativePath=" + enginePath.generic_string();

	engn::EngineInitializationParams engineInitializationParams;
	engineInitializationParams.additionalArgs.emplace_back(enginePathArg);
	engineInitializationParams.additionalArgs.insert(engineInitializationParams.additionalArgs.end(), m_params.additionalArgs.cbegin(), m_params.additionalArgs.cend());
	engn::Engine::Initialize(engineInitializationParams);

	if (m_params.initializeAssetsSystem)
	{
		as::AssetsSystemInitializer assetsSystemInitializer;
		assetsSystemInitializer.contentPath = engn::GetEngine().GetPaths().contentPath;
		assetsSystemInitializer.ddcPath     = engn::GetEngine().GetPaths().enginePath / "DDC";
		engn::GetEngine().GetAssetsSystem().Initialize(assetsSystemInitializer);
	}

	// Nothing else references this translation unit of static Graphics library, so without it linker drops plugin
	gfx::GlobalShaderResourcesManager::Get();

	const Uint32 threadsNum = std::max(std::thread::hardware_concurrency(), 2u);

	js::JobSystemInitializationParams jobSystemParams;
	jobSystemParams.workerThreadsNum = static_cast<SizeType>(threadsNum - 1u);
	js::JobSystem::Initialize(jobSystemParams);

	rdr::GPUApi::Initialize();

	if (m_params.initializeModules)
	{
		engn::EngineGlobals engineGlobals;
		engineGlobals.profiler           = prf::ProfilerCore::GetInstance().GetProfiler();
		engineGlobals.engineInstance     = &engn::Engine::Get();
		engineGlobals.gpuApiData         = rdr::GPUApi::GetGPUApiData();
		engineGlobals.hashedStringDBData = lib::HashedStringDB::GetDBData();
		engn::Engine::Get().GetModulesManager().InitialzeGlobals(engineGlobals);
	}

	rdr::PSOsLibrary::GetInstance().PrecachePSOs(rdr::PSOPrecacheParams{});
}

ScopedEngine::~ScopedEngine()
{
	SPT_PROFILER_FUNCTION();

	if (m_params.initializeAssetsSystem)
	{
		engn::GetEngine().GetAssetsSystem().UnloadPermanentAssets();
	}

	rdr::GPUApi::Uninitialize();

	js::JobSystem::Shutdown();

	if (m_params.initializeModules)
	{
		prf::ProfilerCore::GetInstance().Shutdown();
	}
}

} // spt::tools
