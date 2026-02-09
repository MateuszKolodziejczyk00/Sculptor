#include "Renderer/SandboxRenderer.h"
#include "Common/ShaderCompilationInput.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
#include "Types/Window.h"
#include "Types/RenderContext.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Renderer.h"
#include "CommandsRecorder/RenderingDefinition.h"
#include "Types/Sampler.h"
#include "GPUDiagnose/Debug/GPUDebug.h"
#include "JobSystem.h"
#include "RenderGraphBuilder.h"
#include "Engine.h"
#include "Paths.h"
#include "Transfers/UploadUtils.h"
#include "StaticMeshes/StaticMeshRenderSceneSubsystem.h"
#include "StaticMeshes/StaticMeshesRenderSystem.h"
#include "Lights/LightsRenderSystem.h"
#include "Shadows/ShadowMapsManagerSubsystem.h"
#include "SceneRenderer/SceneRenderer.h"
#include "InputManager.h"
#include "Transfers/TransfersManager.h"
#include "Lights/LightTypes.h"
#include "RayTracing/RayTracingRenderSceneSubsystem.h"
#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"
#include "EngineFrame.h"
#include "Profiler/Profiler.h"
#include "Vulkan/VulkanTypes/RHIQueryPool.h"
#include "DDGI/DDGISceneSubsystem.h"
#include "DDGI/DDGIRenderSystem.h"
#include "Camera/CameraSettings.h"
#include "Loaders/TextureLoader.h"
#include "Atmosphere/AtmosphereSceneSubsystem.h"
#include "Atmosphere/AtmosphereRenderSystem.h"
#include "RenderGraphCapturer.h"
#include "RenderGraphCaptureViewer.h"
#include "RenderGraphCaptureSourceContext.h"
#include "UIElements/ApplicationUI.h"
#include "Shadows/CascadedShadowMapsViewRenderSystem.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"
#include "FileSystem/File.h"
#include "View/Systems/TemporalAAViewRenderSystem.h"
#include "Techniques/TemporalAA/DLSSRenderer.h"
#include "Techniques/TemporalAA/StandardTAARenderer.h"
#include "IESProfileAsset.h"
#include "Assets/MeshAsset/MeshAsset.h"
#include "MaterialsSubsystem.h"
#include "StaticMeshes/RenderMesh.h"
#include "MaterialAsset.h"
#include "PrefabAsset.h"
#include "Transfers/GPUDeferredCommandsQueue.h"

#if SPT_SHADERS_DEBUG_FEATURES
#include "Debug/ShaderDebugUtils.h"
#endif // SPT_SHADERS_DEBUG_FEATURES


namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(Sandbox, true)


SandboxRenderer::SandboxRenderer()
	: m_fovDegrees(90.f)
	, m_nearPlane(0.1f)
	, m_farPlane(1000.f)
	, m_cameraSpeed(5.f)
	, m_renderScene(lib::MakeShared<rsc::RenderScene>())
	, m_captureSourceContext(lib::MakeShared<rg::capture::RGCaptureSourceContext>())
	, m_memoryArena("Sandbox Renderer Arena", 4 * 1024 * 1024, 32 * 1024 * 1024)
{
	InitializeRenderScene();
}

SandboxRenderer::~SandboxRenderer()
{
	// explicit destroy view before scene
	m_renderView.reset();
}

void SandboxRenderer::Update(engn::FrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	math::Vector3f& cameraDeltaLocation = m_cameraDeltaLocation[frame.GetFrameIdx() & 1u];
	math::Vector2f& cameraDeltaRotation = m_cameraDeltaRotation[frame.GetFrameIdx() & 1u];

	cameraDeltaLocation = math::Vector3f::Zero();
	cameraDeltaRotation = math::Vector2f::Zero();

	if (m_isViewportFocused)
	{
		const Real32 deltaTime = frame.GetDeltaTime();

		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::W))
		{
			cameraDeltaLocation += m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * math::Vector3f::UnitX());
		}
		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::S))
		{
			cameraDeltaLocation += m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * -math::Vector3f::UnitX());
		}
		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::D))
		{
			cameraDeltaLocation += m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * math::Vector3f::UnitY());
		}
		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::A))
		{
			cameraDeltaLocation += m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * -math::Vector3f::UnitY());
		}
		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::E))
		{
			cameraDeltaLocation += m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * math::Vector3f::UnitZ());
		}
		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::Q))
		{
			cameraDeltaLocation += m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * -math::Vector3f::UnitZ());
		}

		if (inp::InputManager::Get().IsKeyPressed(inp::EKey::RightMouseButton))
		{
			const math::Vector2f mouseVel = inp::InputManager::Get().GetMouseMoveDelta().cast<Real32>();
			if (mouseVel.squaredNorm() > math::Utils::Square(5.f) * deltaTime)
			{
				const Real32 rotationSpeed = 0.0045f;
				const math::Vector2f rotationDelta = rotationSpeed * mouseVel;

				cameraDeltaRotation += rotationDelta;
			}
		}
	}
}

void SandboxRenderer::UpdatePreRender(engn::FrameContext& frame)
{
	SPT_PROFILER_FUNCTION();

	const Uint64 frameIdx = frame.GetFrameIdx();

	const math::Vector3f& cameraDeltaLocation = m_cameraDeltaLocation[frameIdx & 1u];
	const math::Vector2f& cameraDeltaRotation = m_cameraDeltaRotation[frameIdx & 1u];

	m_renderView->Move(cameraDeltaLocation);

	m_renderView->Rotate(math::AngleAxisf(cameraDeltaRotation.x(), math::Vector3f::UnitZ()));
	m_renderView->Rotate(math::AngleAxisf(cameraDeltaRotation.y(), m_renderView->GetRotation() * math::Vector3f::UnitY()));


	if (sunMovement)
	{
		sunAngleYaw   = pi<Real32> * 0.5f;
		sunAnglePitch = std::fmodf(engn::Engine::Get().GetTime() * sunMovementSpeed, 2.f * pi<Real32>);
		sunAngleDirty = true;
	}

	if (sunAngleDirty)
	{
		sunAngleDirty = false;

		m_directionalLightEntity.patch<rsc::DirectionalLightData>([this](rsc::DirectionalLightData& data)
																  {
																	  const math::Quaternionf pitchQuat = math::Utils::EulerToQuaternionRadians(0.f, sunAnglePitch, 0.f);
																	  const math::Quaternionf yawQuat = math::Utils::EulerToQuaternionRadians(0.f, 0.f, sunAngleYaw);
																	  data.direction = yawQuat * pitchQuat * math::Vector3f::UnitX();
																  });
	}

	if (dirLightTypeDirty)
	{
		dirLightTypeDirty = false;

		m_directionalLightEntity.patch<rsc::DirectionalLightData>([this](rsc::DirectionalLightData& data)
																  {
																	  if (dirLightType == EDirLightType::Sun)
																	  {
																			data.color             = math::Vector3f(1.f, 0.956f, 0.839f);
																			data.sunDiskEC         = 13.4f;
																			data.zenithIlluminance = 120000.f;
																	  }
																	  else
																	  {
																			data.color             = math::Vector3f(0.3f, 0.3f, 0.35f);
																			data.sunDiskEC         = 0.4f;
																			data.zenithIlluminance = 0.1f;
																	  }
																  });

	}
}

void SandboxRenderer::ProcessView(engn::FrameContext& frame, lib::SharedRef<rdr::TextureView> output)
{
	SPT_PROFILER_FUNCTION();

	m_memoryArena.Reset();
	
	UpdatePreRender(frame);

	m_renderScene->BeginFrame(frame);

	PrepareRenderView(output->GetTexture()->GetResolution2D());

	const lib::SharedRef<rdr::GPUStatisticsCollector> gpuStatisticsCollector = lib::MakeShared<rdr::GPUStatisticsCollector>();

	rg::RenderGraphBuilder graphBuilder(m_memoryArena, m_resourcesPool);
	graphBuilder.BindGPUStatisticsCollector(gpuStatisticsCollector);

	rg::capture::RGCaptureSourceInfo captureSource;
	captureSource.sourceContext = m_captureSourceContext;

	rg::capture::CaptureParameters captureParams
	{
		.captureTextures = captureTextures,
		.captureBuffers  = captureBuffers
	};
	rg::capture::RenderGraphCapturer capturer(captureParams, captureSource);

	if (wantsCaptureNextFrame)
	{
		capturer.Capture(graphBuilder);
		wantsCaptureNextFrame = false;
	}

	m_captureSourceContext->ExecuteOnSetupNewGraphBuilder(graphBuilder);

	js::Launch(SPT_GENERIC_JOB_NAME,
			   [resolution = m_renderView->GetRenderingRes(), gpuStatisticsCollector]
			   {
				   prf::GPUProfilerStatistics statistics;
				   statistics.resolution = resolution;
				   statistics.frameStatistics = gpuStatisticsCollector->CollectStatistics();
				   prf::Profiler::Get().SetGPUFrameStatistics(std::move(statistics));
			   },
			   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));

	rsc::SceneRendererSettings rendererSettings;
	rendererSettings.outputFormat = rhi::EFragmentFormat::RGBA8_UN_Float;
	rendererSettings.resetAccumulation = resetAccumulation;

	resetAccumulation = false;

	rg::RGTextureViewHandle sceneRenderingResultTextureView;

	{
#if SPT_SHADERS_DEBUG_FEATURES
		const Bool shaderDebugMousePressed = inp::InputManager::Get().IsKeyPressed(inp::EKey::LShift);

		const Bool enableMouseDebugging = oneFrameDebugMousePosition ? (shaderDebugMousePressed && !m_shaderDebugMousePressedLastFrame) : shaderDebugMousePressed;

		gfx::dbg::ShaderDebugParameters shaderDebugParameters;
		shaderDebugParameters.mouseUV = enableMouseDebugging
			? math::Vector2f((m_mousePositionOnViewport.cast<Real32>() + math::Vector2f::Constant(0.5f)).cwiseQuotient(output->GetResolution2D().cast<Real32>()))
			: math::Vector2f::Constant(-2.f);
		shaderDebugParameters.viewportSize = output->GetResolution2D();
		shaderDebugParameters.resetPersistentDebugGeometry = resetPersistentDebugs;
		const gfx::dbg::ShaderDebugScope shaderDebugCommandsCollectingScope(graphBuilder, shaderDebugParameters);
		resetPersistentDebugs = false;
		m_shaderDebugMousePressedLastFrame = shaderDebugMousePressed;

		rendererSettings.debugUAVTexture         = shaderDebugCommandsCollectingScope.GetDebugOutputTexture();
		rendererSettings.dynamicDebugRenderer    = &shaderDebugCommandsCollectingScope.GetDynamicDebugRenderer();
		rendererSettings.persistentDebugRenderer = &shaderDebugCommandsCollectingScope.GetPersistentDebugRenderer();
#endif // SPT_SHADERS_DEBUG_FEATURES

		sceneRenderingResultTextureView = m_sceneRenderer.Render(graphBuilder, *m_renderScene, *m_renderView, rendererSettings);
	}

	SPT_CHECK(sceneRenderingResultTextureView.IsValid());

	const rg::RGTextureViewHandle sceneUItextureView = graphBuilder.AcquireExternalTextureView(output);

	if (wantsToCreateScreenshot)
	{
		const lib::String screenshotFileName = lib::File::Utils::CreateFileNameFromTime("png");
		const lib::String screenshotFilePath = engn::Paths::Combine(engn::Paths::GetSavedPath(), engn::Paths::Combine("Screenshots", screenshotFileName));
		SPT_LOG_INFO(Sandbox, "Writing screenshot to {}", screenshotFilePath);
		js::JobWithResult<Bool> saveJob = gfx::TextureWriter::SaveTexture(graphBuilder, sceneRenderingResultTextureView, screenshotFilePath);

		js::Launch(SPT_GENERIC_JOB_NAME,
				   [saveJob, screenshotFilePath]
				   {
					   const Bool saveResult = saveJob.GetResult();
					   if (saveResult)
					   {
						   SPT_LOG_INFO(Sandbox, "Screenshot saved to {}", screenshotFilePath);
					   }
					   else
					   {
						   SPT_LOG_ERROR(Sandbox, "Failed to save screenshot to {}", screenshotFilePath);
					   }

				   },
				   js::Prerequisites(saveJob));

		wantsToCreateScreenshot = false;
	}

	graphBuilder.CopyTexture(RG_DEBUG_NAME("Copy Scene Result Texture"),
							 sceneRenderingResultTextureView, math::Vector3i::Zero(),
							 sceneUItextureView, math::Vector3i::Zero(),
							 sceneRenderingResultTextureView->GetResolution());

	// prepare for UI pass
	graphBuilder.ReleaseTextureWithTransition(sceneUItextureView->GetTexture(), rhi::TextureTransition::ShaderRead);

	graphBuilder.Execute();

	if (capturer.HasValidCapture())
	{
		js::Launch(SPT_GENERIC_JOB_NAME,
				   [rgCapture = lib::Ref(capturer.GetCapture())]
				   {
					   scui::ViewDefinition viewDef("Render Graph Capture Viewer");
					   viewDef.minimumSize = math::Vector2u(800, 600);
					   lib::SharedRef<rg::capture::RenderGraphCaptureViewer> captureView = lib::MakeShared<rg::capture::RenderGraphCaptureViewer>(viewDef, std::move(rgCapture));
					   scui::ApplicationUI::AddView(std::move(captureView));
				   },
				   js::Prerequisites(graphBuilder.GetGPUFinishedEvent()));
	}

	m_renderScene->EndFrame();
}

rsc::SceneRenderer& SandboxRenderer::GetSceneRenderer()
{
	return m_sceneRenderer;
}

const lib::SharedPtr<rsc::RenderScene>& SandboxRenderer::GetRenderScene()
{
	return m_renderScene;
}

const lib::SharedPtr<rsc::RenderView>& SandboxRenderer::GetRenderView() const
{
	return m_renderView;
}

void SandboxRenderer::SetFov(Real32 fovDegrees)
{
	m_fovDegrees = fovDegrees;
}

Real32 SandboxRenderer::GetFov()
{
	return m_fovDegrees;
}

void SandboxRenderer::SetNearPlane(Real32 nearPlane)
{
	m_nearPlane = nearPlane;
}

Real32 SandboxRenderer::GetNearPlane()
{
	return m_nearPlane;
}

void SandboxRenderer::SetFarPlane(Real32 farPlane)
{
	m_farPlane = farPlane;
}

Real32 SandboxRenderer::GetFarPlane()
{
	return m_farPlane;
}

void SandboxRenderer::SetCameraSpeed(Real32 speed)
{
	m_cameraSpeed = speed;
}

Real32 SandboxRenderer::GetCameraSpeed()
{
	return m_cameraSpeed;
}

void SandboxRenderer::SetMousePositionOnViewport(const math::Vector2i& mousePosition)
{
	m_mousePositionOnViewport = mousePosition;
}

void SandboxRenderer::SetViewportFocused(Bool isFocused)
{
	m_isViewportFocused = isFocused;
}

void SandboxRenderer::CreateRenderGraphCapture()
{
	m_wantsCaptureNextFrame = true;
}

void SandboxRenderer::CreateScreenshot()
{
	m_wantsToCreateScreenshot = true;
}

void SandboxRenderer::InitializeRenderScene()
{
	SPT_PROFILER_FUNCTION();

	const js::JobWithResult dlssInitJob = js::Launch(SPT_GENERIC_JOB_NAME,
													 []()
													 {
														 return gfx::DLSSRenderer::InitializeDLSS();
													 });

	as::AssetsSystem& assetsSystem = engn::Engine::Get().GetAssetsSystem();

	as::IESProfileAssetHandle iesProfile0 = assetsSystem.LoadAssetChecked<as::IESProfileAsset>(as::ResourcePath("IESProfiles/IESProfile0.sptasset"));
	as::IESProfileAssetHandle iesProfile1 = assetsSystem.LoadAssetChecked<as::IESProfileAsset>(as::ResourcePath("IESProfiles/IESProfile1.sptasset"));

	as::PrefabAssetHandle sponzaPrefab = assetsSystem.LoadAssetChecked<as::PrefabAsset>(as::ResourcePath("Sponza/Sponza.sptasset"));

	as::IESProfileAssetHandle iesProfiles[] = { iesProfile0, iesProfile1 };

	m_renderView = lib::MakeShared<rsc::RenderView>(*m_renderScene);
	m_renderView->SetLocation(math::Vector3f(0.f, 0.f, 1.f));

	m_renderView->AddRenderStages(rsc::ERenderStage::DeferredRendererStages);
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		m_renderView->AddRenderStages(rsc::ERenderStage::RayTracingRenderStages);
	}
	m_renderView->SetOutputRes(math::Vector2u(1920, 1080));
	m_renderView->SetPerspectiveProjection(math::Utils::DegreesToRadians(m_fovDegrees), 1920.f / 1080.f, m_nearPlane, m_farPlane);

	rsc::ShadowCascadesParams cascadesParams;
	cascadesParams.shadowsTechnique = rsc::EShadowMappingTechnique::DPCF;
	m_renderView->AddRenderSystem<rsc::CascadedShadowMapsViewRenderSystem>(cascadesParams);
	m_renderView->AddRenderSystem<rsc::ParticipatingMediaViewRenderSystem>();

	if (lib::SharedPtr<rdr::Texture> lensDirtTexture = gfx::TextureLoader::LoadTexture(engn::Paths::Combine(engn::Paths::GetContentPath(), "Camera/LensDirt.jpeg"), lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest)))
	{
		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);

		rsc::CameraLensSettingsComponent cameraLensSettingsComponent;
		cameraLensSettingsComponent.lensDirtTexture = lensDirtTexture->CreateView(RENDERER_RESOURCE_NAME("Lens Dirt View"), viewDef);
		m_renderView->GetBlackboard().Create<rsc::CameraLensSettingsComponent>(cameraLensSettingsComponent);
	}

	m_renderScene->AddSceneSubsystem<rsc::StaticMeshRenderSceneSubsystem>();
	m_renderScene->AddSceneSubsystem<rsc::ShadowMapsManagerSubsystem>(m_renderView);
	m_renderScene->AddSceneSubsystem<rsc::AtmosphereSceneSubsystem>();
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		m_renderScene->AddSceneSubsystem<rsc::RayTracingRenderSceneSubsystem>();
		
		m_renderScene->AddRenderSystem<rsc::ddgi::DDGIRenderSystem>();
		m_renderScene->AddSceneSubsystem<rsc::ddgi::DDGISceneSubsystem>();
	}
	m_renderScene->AddRenderSystem<rsc::StaticMeshesRenderSystem>();
	m_renderScene->AddRenderSystem<rsc::LightsRenderSystem>();
	m_renderScene->AddRenderSystem<rsc::AtmosphereRenderSystem>();

	{
		const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
		rsc::PointLightData pointLightData;
		pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
		pointLightData.luminousPower = 3200.f;
		pointLightData.location = math::Vector3f(8.30f, 2.6f, 1.55f);
		pointLightData.radius = 5.f;
		pointLightData.iesProfileTexture = iesProfile0->GetTextureView();
		lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
	}

	{
		const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
		rsc::PointLightData pointLightData;
		pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
		pointLightData.luminousPower = 3200.f;
		pointLightData.location = math::Vector3f(-8.30f, 3.9f, 4.45f);
		pointLightData.radius = 5.f;
		pointLightData.iesProfileTexture = iesProfile0->GetTextureView();
		lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
	}

	{
		const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
		rsc::PointLightData pointLightData;
		pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
		pointLightData.luminousPower = 3200.f;
		pointLightData.location = math::Vector3f(8.30f, -3.8f, 1.55f);
		pointLightData.radius = 5.f;
		pointLightData.iesProfileTexture = iesProfile0->GetTextureView();
		lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
	}
	
	{
		const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
		rsc::PointLightData pointLightData;
		pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
		pointLightData.luminousPower = 3200.f;
		pointLightData.location = math::Vector3f(-4.24f, -14.85f, 2.05f);
		pointLightData.radius = 8.f;
		pointLightData.iesProfileTexture = iesProfile0->GetTextureView();
		lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
	}

	{
		m_directionalLightEntity = m_renderScene->CreateEntity();
		rsc::DirectionalLightData directionalLightData;
		directionalLightData.color                  = math::Vector3f(1.f, 0.956f, 0.839f);
		directionalLightData.zenithIlluminance      = 120000.f;
		directionalLightData.direction              = math::Vector3f(-0.5f, -0.3f, -1.7f).normalized();
		directionalLightData.lightConeAngle         = 0.0046f;
		directionalLightData.sunDiskAngleMultiplier = 3.8f;
		directionalLightData.sunDiskEC              = 13.4f;
		m_directionalLightEntity.emplace<rsc::DirectionalLightData>(directionalLightData);
	}

	dirLightTypeDirty = true;
	sunAngleDirty = true;
	for (const auto& iesProfile : iesProfiles)
	{
		iesProfile->AwaitInitialization();
	}
	sponzaPrefab->AwaitInitialization();
	sponzaPrefab->SetPermanent();

	sponzaPrefab->Spawn(as::PrefabSpawnParams
						{
							.scene = *m_renderScene,
							.rotation = math::Vector3f(90.f, 0.f, 0.f)
						});

	gfx::GPUDeferredCommandsQueue::Get().ForceFlushCommands();

	const Bool dlssInitResult = dlssInitJob.Await();

	auto dlssRenderer = dlssInitResult ? std::make_unique<gfx::DLSSRenderer>() : nullptr;
	rsc::TemporalAAViewRenderSystem* temporalAAViewSystem = m_renderView->AddRenderSystem<rsc::TemporalAAViewRenderSystem>();
	SPT_CHECK(!!temporalAAViewSystem);
	const gfx::TemporalAAInitSettings aaInitSettings{};
	if (dlssRenderer && dlssRenderer->Initialize(aaInitSettings))
	{
		temporalAAViewSystem->SetTemporalAARenderer(std::move(dlssRenderer));
	}
	else
	{
		auto standardTAArenderer = std::make_unique<gfx::StandardTAARenderer>();

		SPT_MAYBE_UNUSED
		const Bool initResult = standardTAArenderer->Initialize(aaInitSettings);
		SPT_CHECK(initResult);

		temporalAAViewSystem->SetTemporalAARenderer(std::move(standardTAArenderer));
	}

	SPT_LOG_INFO(Sandbox, "Render Scene Initialization Finished");
}

void SandboxRenderer::PrepareRenderView(math::Vector2u outputResolution)
{
	m_renderView->SetOutputRes(outputResolution);

	const Real32 defaultAspectRatio = 16.f / 9.f;

	const Real32 imageAspectRatio = static_cast<Real32>(outputResolution.x()) / static_cast<Real32>(outputResolution.y());

	const Real32 renderingFov = m_fovDegrees * std::min<Real32>((imageAspectRatio / defaultAspectRatio), 1.f);
	const Real32 renderingFovRadians = math::Utils::DegreesToRadians(renderingFov);
	const Real32 reneringAspect = static_cast<Real32>(outputResolution.x()) / static_cast<Real32>(outputResolution.y());

	m_renderView->SetPerspectiveProjection(renderingFovRadians, reneringAspect, m_nearPlane, m_farPlane);
}

} // spt::ed
