#include "ScopedEngine.h"
#include "Engine.h"
#include "EngineFrame.h"
#include "GPUApi.h"
#include "JobSystem.h"
#include "World.h"
#include "PrefabAsset.h"
#include "RenderScene.h"
#include "View/RenderView.h"
#include "SceneRenderer/SceneRenderer.h"
#include "Lights/LightTypes.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResourcesPool.h"
#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"
#include "Loaders/TextureLoader.h"
#include "Transfers/GPUDeferredCommandsQueue.h"
#include "Utils/TransfersUtils.h"
#include "FileSystem/File.h"
#include "Serialization.h"
#include "SerializationHelper.h"
#include "ThreadContext.h"
#include "RGOutputHasher.h"

#if SPT_SHADERS_DEBUG_FEATURES
#include "Debug/ShaderDebugUtils.h"
#endif // SPT_SHADERS_DEBUG_FEATURES


SPT_DEFINE_LOG_CATEGORY(RenderTests, true)

namespace spt::render_tests
{

struct TestSuiteState
{
	lib::ManagedSpan<js::JobWithResult<Bool>> saveTextureJobs;
	Uint32 currentTestIdx = 0u;
};


TestSuiteState g_testSuiteState;


class TestFrameFinishedWaitable : public lib::Waitable
{
public:

	TestFrameFinishedWaitable(js::Event gpuFinishedEvent)
		: m_gpuFinishedEvent(std::move(gpuFinishedEvent))
	{ }

	virtual void Wait() override
	{
		m_gpuFinishedEvent.Wait();
	}

private:

	js::Event m_gpuFinishedEvent;
};


struct SceneTestSpawn
{
	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Location", location);
		serializer.Serialize("Rotation", rotation);
		serializer.Serialize("Scale", scale);
	}

	math::Vector3f location = math::Vector3f::Zero();
	math::Vector3f rotation = math::Vector3f::Zero();
	math::Vector3f scale    = math::Vector3f::Ones();
};


struct SceneTestCamera
{
	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Location", location);
		serializer.Serialize("Rotation", rotation);
		serializer.Serialize("FOV", fovDegrees);
		serializer.Serialize("Near", nearPlane);
		serializer.Serialize("Far", farPlane);
	}

	math::Vector3f location   = math::Vector3f::Zero();
	math::Vector3f rotation   = math::Vector3f::Zero(); // Euler radians, same as SandboxCameraConfig
	Real32         fovDegrees = 90.f;
	Real32         nearPlane  = 0.1f;
	Real32         farPlane   = 1000.f;
};


struct SceneTestDefinition
{
	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Type", type);
		serializer.Serialize("Prefab", prefab);
		serializer.Serialize("Spawn", spawn);
		serializer.Serialize("Camera", camera);
		serializer.Serialize("Resolution", resolution);
		serializer.Serialize("WarmupFrames", warmupFrames);
		serializer.Serialize("PerfFrames", perfFrames);
		serializer.Serialize("ViewSettings", viewSettings);
		serializer.Serialize("SceneSettings", sceneSettings);
	}

	// Golden frame is rendered last, in render graph test mode (clears and hashes) which affects its timings
	Uint32 GetFramesNum() const { return warmupFrames + perfFrames + 1u; }

	lib::String name;

	lib::String type = "Scene";

	lib::String     prefab;
	SceneTestSpawn  spawn;
	SceneTestCamera camera;

	math::Vector2u resolution = math::Vector2u(1920u, 1080u);

	lib::String upscaling = "TAA";

	Uint32 warmupFrames = 16u;
	Uint32 perfFrames   = 32u;

	rsc::RenderViewSettings viewSettings;
	rsc::RenderSceneSettings sceneSettings;
};


SceneTestDefinition LoadSceneTestDefinition(const lib::Path& path)
{
	SceneTestDefinition def;
	ApplyTestDefaults(def.viewSettings);
	ApplyTestDefaults(def.sceneSettings);
	const Bool loaded = srl::SerializationHelper::LoadTextStructFromFile(def, path.generic_string());
	SPT_CHECK_MSG(loaded, "Failed to load test {}", path.generic_string());
	SPT_CHECK_MSG(def.perfFrames > 0u, "Test must collect performance from at least one frame ({})", path.generic_string());
	def.name = path.stem().generic_string();
	return def;
}


void AppendScopeStatistics(const rdr::GPUStatisticsScopeData& scope, const lib::String& parentPath, srl::JSON& outScopes)
{
	const lib::String path = parentPath.empty() ? scope.name.ToString() : parentPath + "/" + scope.name.ToString();

	srl::JSON scopeJson;
	scopeJson["Path"]       = path;
	scopeJson["DurationMs"] = scope.GetDurationInMs();

	if (scope.fragmentShaderInvocations)
	{
		scopeJson["FSInvocations"] = *scope.fragmentShaderInvocations;
	}

	if (scope.computeShaderInvocations)
	{
		scopeJson["CSInvocations"] = *scope.computeShaderInvocations;
	}

	if (scope.taskShaderInvocations)
	{
		scopeJson["TSInvocations"] = *scope.taskShaderInvocations;
	}

	for (const rhi::PipelineStatistic& statistic : scope.psoStatistics.statistics)
	{
		std::visit([&](const auto& value) { scopeJson["PipelineStatistics"][statistic.name] = value; }, statistic.value);
	}

	if (scope.transientMemoryUsage)
	{
		scopeJson["TransientMemoryUsage"] = *scope.transientMemoryUsage;
	}

	for (const rdr::TransientAllocEventInfo& allocation : scope.transientAllocations)
	{
		scopeJson["TransientAllocations"].push_back({ { "Name", allocation.name.ToString() }, { "Size", allocation.size }, { "Id", allocation.allocationId } });
	}

	for (const rdr::TransientAllocEventInfo& deallocation : scope.transientDeallocations)
	{
		scopeJson["TransientDeallocations"].push_back({ { "Name", deallocation.name.ToString() }, { "Size", deallocation.size }, { "Id", deallocation.allocationId } });
	}

	outScopes.push_back(std::move(scopeJson));

	for (const rdr::GPUStatisticsScopeData& child : scope.children)
	{
		AppendScopeStatistics(child, path, outScopes);
	}
}


class SceneTest
{
public:

	SceneTest(const SceneTestDefinition& definition, const lib::Path& outputDirectory);
	~SceneTest();

	void Run();

private:

	void InitializeScene();

	void ExecuteFrame(Uint32 frameIdx);

	void RenderView(engn::FrameContext& frame, Uint32 perfStatisticsFrameIdx, Bool captureResult);

	const SceneRendererDLLModuleAPI& GetSceneRendererAPI() const;

	const SceneTestDefinition& m_definition;
	lib::Path                  m_outputDirectory;

	lib::SharedPtr<gf::World>    m_world;
	rsc::SceneRendererHandle     m_sceneRenderer;
	rsc::RenderView*             m_renderView = nullptr;

	lib::MemoryArena             m_memoryArena;
	rg::RenderGraphResourcesPool m_resourcesPool;

	lib::SharedPtr<rdr::GPUStatisticsCollector> m_statisticsCollector;

	js::Event m_perfCollectionFinishedEvent;

#if RG_ENABLE_TEST_MODE
	std::optional<rg::test_mode::OutputsHasher> m_outputsHasher;
#endif // RG_ENABLE_TEST_MODE

	srl::JSON m_perfFrames = srl::JSON::array();
};


SceneTest::SceneTest(const SceneTestDefinition& definition, const lib::Path& outputDirectory)
	: m_definition(definition)
	, m_outputDirectory(outputDirectory / definition.name)
	, m_world(lib::MakeShared<gf::World>())
	, m_memoryArena("Render Test Arena", 4u * 1024u * 1024u, 32u * 1024u * 1024u)
{
	std::filesystem::create_directories(m_outputDirectory);

	InitializeScene();

	m_resourcesPool.Preallocate(rhi::RHIAllocationInfo(rhi::EMemoryUsage::GPUOnly), 256u * 1024u * 1024u);
}

SceneTest::~SceneTest()
{
	rdr::GPUApi::WaitIdle();

	// explicit destroy view before scene
	GetSceneRendererAPI().DestroyRenderView(m_renderView);
	GetSceneRendererAPI().DestroySceneRenderer(m_sceneRenderer);

	m_world.reset();

	rdr::GPUApi::WaitIdle();

	engn::EngineFramesManager::Shutdown();
	engn::EngineFramesManager::ResetTimeline();
}

void SceneTest::Run()
{
	SPT_PROFILER_FUNCTION();

	TCX_SCRATCH_SCOPE();

	SPT_LOG_INFO(RenderTests, "Running test {}", m_definition.name);

	// Resize
	m_perfFrames[m_definition.perfFrames - 1u];

	m_perfCollectionFinishedEvent = js::CreateEvent("Perf Collection Finished Event");

	for (Uint32 frameIdx = 0u; frameIdx < m_definition.GetFramesNum(); ++frameIdx)
	{
		ExecuteFrame(frameIdx);
	}

	rdr::GPUApi::WaitIdle();

	m_perfCollectionFinishedEvent.Signal();
	m_perfCollectionFinishedEvent.Wait(); // Wait for all jobs

#if RG_ENABLE_TEST_MODE
	SPT_CHECK(m_outputsHasher.has_value());

	srl::JSON passesJson = srl::JSON::array();
	for (const rg::test_mode::PassOutputsHashes& pass : m_outputsHasher->ComputeHashes(tcx::Scratch()))
	{
		srl::JSON outputsJson = srl::JSON::array();
		for (const rg::test_mode::OutputHash& output : pass.outputs)
		{
			outputsJson.push_back({ { "Name", output.name.GetView() }, { "Hash", std::format("{:016x}", output.hash) } });
		}

		passesJson.push_back({ { "Name", pass.name.GetView() }, { "Outputs", std::move(outputsJson) } });
	}

	lib::File::SaveDocument(m_outputDirectory / "Hashes.json", srl::JSON{ { "Passes", std::move(passesJson) } }.dump(1, '\t'));
#endif // RG_ENABLE_TEST_MODE

	srl::Serializer settingsWriter = srl::Serializer::CreateWriter();
	settingsWriter.Serialize("ViewSettings", m_renderView->settings);
	settingsWriter.Serialize("SceneSettings", m_world->renderSceneSettings);
	lib::File::SaveDocument(m_outputDirectory / "Settings.json", settingsWriter.ToString());

	srl::JSON perf;
	perf["Test"]       = m_definition.name;
	perf["Resolution"] = { m_definition.resolution.x(), m_definition.resolution.y() };
	perf["Frames"]     = std::move(m_perfFrames);

	// Written last, so its existence marks test as finished
	lib::File::SaveDocument(m_outputDirectory / "Perf.json", perf.dump(1, '\t'));

	SPT_LOG_INFO(RenderTests, "Finished test {}", m_definition.name);
}

void SceneTest::InitializeScene()
{
	SPT_PROFILER_FUNCTION();

	as::AssetsSystem& assetsSystem = engn::Engine::Get().GetAssetsSystem();

	as::PrefabAssetHandle prefab = assetsSystem.LoadAssetChecked<as::PrefabAsset>(as::ResourcePath(m_definition.prefab));

	// Volumetric clouds renderer requires cloudscape to be set
	as::CloudscapeAssetHandle cloudscape = assetsSystem.LoadAssetChecked<as::CloudscapeAsset>(as::ResourcePath("Cloudscape/Cloudscape.sptasset"));
	cloudscape->AwaitInitialization();
	m_world->SetCloudscape(cloudscape);

	m_world->renderSceneSettings = m_definition.sceneSettings;

	{
		rsc::DirectionalLightData directionalLightData;
		directionalLightData.color                  = math::Vector3f(1.f, 0.956f, 0.839f);
		directionalLightData.zenithIlluminance      = 120000.f;
		directionalLightData.direction              = math::Vector3f(-0.5f, -0.3f, -1.7f).normalized();
		directionalLightData.lightConeAngle         = 0.0046f;
		directionalLightData.sunDiskAngleMultiplier = 3.8f;
		directionalLightData.sunDiskEC              = 13.4f;
		m_world->GetRenderScene()->lighting.SetDirectionalLight(directionalLightData);
	}

	prefab->AwaitInitialization();
	m_world->SpawnPrefab(prefab, gf::SpawnParams{ .location = m_definition.spawn.location, .rotation = m_definition.spawn.rotation, .scale = m_definition.spawn.scale });

	const SceneRendererDLLModuleAPI& sceneRendererAPI = GetSceneRendererAPI();

	const rsc::SceneRendererDefinition sceneRendererDef
	{
		.scene         = m_world->GetRenderSceneRef(),
		.renderSystems = rsc::ESceneRenderSystem::ALL
	};
	m_sceneRenderer = sceneRendererAPI.CreateSceneRenderer(sceneRendererDef);

	rsc::RenderViewDefinition viewDef;
	viewDef.renderStages  = rsc::ERenderStage::DeferredRendererStages;
	viewDef.renderSystems = rsc::EViewRenderSystem::ALL;

	if (rdr::GPUApi::IsRayTracingEnabled())
	{
		lib::AddFlags(viewDef.renderStages, rsc::ERenderStage::RayTracingRenderStages);
	}

	const math::Vector2u resolution = m_definition.resolution;

	m_renderView = sceneRendererAPI.CreateRenderView(viewDef);
	const SceneTestCamera& camera = m_definition.camera;
	m_renderView->SetLocation(camera.location);
	m_renderView->SetRotation(math::Utils::EulerToQuaternionRadians(camera.rotation.x(), camera.rotation.y(), camera.rotation.z()));
	m_renderView->SetOutputRes(resolution);
	m_renderView->SetPerspectiveProjection(math::Utils::DegreesToRadians(camera.fovDegrees), static_cast<Real32>(resolution.x()) / static_cast<Real32>(resolution.y()), camera.nearPlane, camera.farPlane);

	m_renderView->settings = m_definition.viewSettings;

	lib::MemoryArena tempArena("Render Test Init Arena", 0u, 16u * 1024u * 1024u);
	gfx::GPUDeferredCommandsQueue& commandsQueue = engn::GetEngine().GetPluginsManager().GetPluginChecked<gfx::GPUDeferredCommandsQueue>();
	commandsQueue.ForceFlushCommands(tempArena);
}

void SceneTest::ExecuteFrame(Uint32 frameIdx)
{
	SPT_PROFILER_FUNCTION();

	TCX_SCRATCH_SCOPE();

	const Bool captureResult     = frameIdx + 1u == m_definition.GetFramesNum();
	const Bool collectStatistics = !captureResult && frameIdx >= m_definition.warmupFrames;
	const Uint32 perfStatisticsFrameIdx = collectStatistics ? frameIdx - m_definition.warmupFrames : idxNone<Uint32>;

	const lib::SharedPtr<engn::FrameContext> frame = engn::EngineFramesManager::CreateFrame<engn::FrameContext>();

	rdr::GPUApi::GetDeviceQueuesManager().AdvanceGPUTimelineSection();
	rdr::GPUApi::FlushCaches();
	rdr::GPUApi::ScheduleFlushDeferredReleases();
	rdr::GPUApi::FlushReadyReleases();

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [this, &frame]
			   {
				   m_world->BeginFrame(*frame);
			   },
			   js::JobDef()
				   .SetPriority(js::EJobPriority::High)
				   .ExecuteBefore(frame->GetStageBeginEvent(engn::EFrameStage::TransferDataForRendering)));

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [this, &frame, perfStatisticsFrameIdx, captureResult]
			   {
				   RenderView(*frame, perfStatisticsFrameIdx, captureResult);
			   },
			   js::Prerequisites(frame->GetStageBeginEvent(engn::EFrameStage::ProcessViewsRendering)),
			   js::JobDef()
				   .SetPriority(js::EJobPriority::High)
				   .ExecuteBefore(frame->GetStageFinishedEvent(engn::EFrameStage::ProcessViewsRendering)));

	frame->BeginAdvancingStages();
	frame->WaitRenderingEnded();

	constexpr Bool allowAsync = true;
	if (captureResult || !allowAsync)
	{
		frame->WaitFrameFinished();
	}
}


void SceneTest::RenderView(engn::FrameContext& frame, Uint32 perfStatisticsFrameIdx, Bool captureResult)
{
	SPT_PROFILER_FUNCTION();

	m_memoryArena.Reset();

	m_world->GetRenderScene()->BeginFrame(frame);

	rg::RenderGraphBuilder graphBuilder(m_memoryArena, m_resourcesPool);

#if RG_ENABLE_TEST_MODE
	if (captureResult)
	{
		graphBuilder.EnableTestMode();
		m_outputsHasher.emplace().Attach(graphBuilder);
	}
#endif // RG_ENABLE_TEST_MODE

	const Bool collectStatistics = perfStatisticsFrameIdx != idxNone<Uint32>;

	if (collectStatistics)
	{
		m_statisticsCollector = lib::MakeShared<rdr::GPUStatisticsCollector>().ToSharedPtr();
		m_statisticsCollector->SetCollectMemoryDetails(true);
		graphBuilder.BindGPUStatisticsCollector(lib::ToSharedRef(m_statisticsCollector));
	}

	rsc::SceneRendererSettings rendererSettings{ frame };
	rendererSettings.outputFormat      = rhi::EFragmentFormat::RGBA8_UN_Float;
	rendererSettings.resetAccumulation = frame.GetFrameIdx() < 2u;

	rg::RGTextureViewHandle resultTextureView;

	{
#if SPT_SHADERS_DEBUG_FEATURES
		// Shaders compiled with debug features expect shader debug params to be bound
		gfx::dbg::ShaderDebugParameters shaderDebugParameters;
		shaderDebugParameters.mouseUV      = math::Vector2f::Constant(-2.f);
		shaderDebugParameters.viewportSize = m_definition.resolution;
		const gfx::dbg::ShaderDebugScope shaderDebugScope(graphBuilder, shaderDebugParameters);

		rendererSettings.debugUAVTexture         = shaderDebugScope.GetDebugOutputTexture();
		rendererSettings.dynamicDebugRenderer    = &shaderDebugScope.GetDynamicDebugRenderer();
		rendererSettings.persistentDebugRenderer = &shaderDebugScope.GetPersistentDebugRenderer();
#endif // SPT_SHADERS_DEBUG_FEATURES

		resultTextureView = GetSceneRendererAPI().ExecuteSceneRendering(m_sceneRenderer, graphBuilder, m_world->GetRenderSceneRef(), *m_renderView, rendererSettings);
	}

	SPT_CHECK(resultTextureView.IsValid());

	if (captureResult)
	{
		g_testSuiteState.saveTextureJobs[g_testSuiteState.currentTestIdx] = gfx::TextureWriter::SaveTexture(graphBuilder, resultTextureView, (m_outputDirectory / "Result.png").generic_string());
	}

	rdr::FlushPendingUploads();

	graphBuilder.Execute();

	frame.SetFrameFinishedOnGPUWaitable(lib::MakeShared<TestFrameFinishedWaitable>(graphBuilder.GetGPUFinishedEvent()));

	const rhi::RHIMemoryUsageStats memoryStats = rhi::RHI::GetMemoryUsageStats();

	if (collectStatistics)
	{
		js::Launch(SPT_GENERIC_JOB_NAME,
				   [this, statisticsCollector = std::move(m_statisticsCollector), frameIdx = frame.GetFrameIdx(), perfStatisticsFrameIdx, memoryStats]
				   {
						rdr::GPUStatisticsData frameStatistics;
						statisticsCollector->CollectStatistics(OUT frameStatistics);

						srl::JSON scopes = srl::JSON::array();
						AppendScopeStatistics(*frameStatistics.fullScope, lib::String(), scopes);

						srl::JSON frameJson;
						frameJson["FrameIdx"]   = frameIdx;
						frameJson["GPUFrameMs"] = frameStatistics.fullScope->GetDurationInMs();
						frameJson["Scopes"]     = std::move(scopes);
						frameJson["MemoryStats"] = { { "TotalAllocatedBytes", memoryStats.totalAllocatedBytes } };
						m_perfFrames[perfStatisticsFrameIdx] = std::move(frameJson);
				   },
				   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()),
				   js::JobDef()
					   .SetPriority(js::EJobPriority::High)
					   .ExecuteBefore(m_perfCollectionFinishedEvent));
	}

	m_world->GetRenderScene()->EndFrame();
}


const SceneRendererDLLModuleAPI& SceneTest::GetSceneRendererAPI() const
{
	const SceneRendererDLLModuleAPI* api = engn::Engine::Get().GetModulesManager().GetModuleAPI<SceneRendererDLLModuleAPI>();
	SPT_CHECK(!!api);
	return *api;
}


lib::ManagedSpan<lib::Path> LoadTestsList(const lib::Path& listPath)
{
	const lib::Span<char> doc = lib::File::ReadDocument(tcx::Scratch(), listPath);

	Uint32 testsNum = 0u;

	SizeType lineStart = 0u;
	for (SizeType i = 0u; i < doc.size(); ++i)
	{
		if (doc[i] == '\n' || doc[i] == '\r' || i + 1u == doc.size())
		{
			if (i > lineStart)
			{
				++testsNum;
			}

			lineStart = i + 1u;
		}
	}

	SPT_CHECK_MSG(testsNum > 0u, "No tests found in {}", listPath.generic_string());

	lib::ManagedSpan<lib::Path> tests = tcx::Scratch().AllocateArray<lib::Path>(testsNum);

	lineStart = 0u;
	SizeType testIdx = 0u;
	for (SizeType i = 0u; i < doc.size(); ++i)
	{
		if (doc[i] == '\n' || doc[i] == '\r' || i + 1u == doc.size())
		{
			if (i > lineStart)
			{
				SizeType length = i - lineStart;
				if (doc[i] != '\n' && doc[i] != '\r' && i + 1u == doc.size())
				{
					++length;
				}

				tests[testIdx++] = lib::Path(lib::StringView(doc.data() + lineStart, length));
			}

			lineStart = i + 1u;
		}
	}

	return tests;
}

} // spt::render_tests


int main()
{
	using namespace spt;

	tools::ScopedEngineParams engineParams;
	engineParams.initializeAssetsSystem = true;
	engineParams.initializeModules      = true;
	engineParams.additionalArgs.emplace_back("-FixedDeltaTime=0.0166667");

	const tools::ScopedEngine engine(engineParams);

	SPT_LOG_INFO(RenderTests, "Render tests started");

	const engn::CommandLineArguments& cmdLineArgs = engn::Engine::Get().GetCmdLineArgs();
	SPT_CHECK_MSG((cmdLineArgs.Contains("-TestsList") || cmdLineArgs.Contains("-Test")) && cmdLineArgs.Contains("-Output"), "Usage: RenderTests (-TestsList=<file with test json path per line> | -Test=<test json path>) -Output=<directory>");

	const lib::Path outputDirectory = cmdLineArgs.GetValue("-Output").ToString();

	lib::ManagedSpan<lib::Path> tests;
	if (cmdLineArgs.Contains("-Test"))
	{
		tests = tcx::Scratch().AllocateArray<lib::Path>(1u);
		tests[0] = lib::Path(cmdLineArgs.GetValue("-Test").ToString());
	}
	else
	{
		tests = render_tests::LoadTestsList(cmdLineArgs.GetValue("-TestsList").ToString());
	}

	render_tests::g_testSuiteState.saveTextureJobs = tcx::Scratch().AllocateArray<js::JobWithResult<Bool>>(tests.Size());

	engn::Engine::Get().GetModulesManager().LoadModule("SceneRenderer");

	for (const lib::Path& testPath : tests)
	{
		SPT_LOG_INFO(RenderTests, "Running test {}", testPath.generic_string());

		const render_tests::SceneTestDefinition definition = render_tests::LoadSceneTestDefinition(testPath);

		render_tests::SceneTest test(definition, outputDirectory);
		test.Run();

		render_tests::g_testSuiteState.currentTestIdx++;
	}

	for (js::JobWithResult<Bool>& job : render_tests::g_testSuiteState.saveTextureJobs)
	{
		SPT_CHECK_MSG(job.Await(), "Failed to save result image");
	}

	render_tests::g_testSuiteState.saveTextureJobs = lib::ManagedSpan<js::JobWithResult<Bool>>();

	return 0;
}
