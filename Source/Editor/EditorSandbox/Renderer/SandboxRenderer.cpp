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
#include "GPUDiagnose/Profiler/GPUProfiler.h"
#include "GPUDiagnose/Debug/GPUDebug.h"
#include "JobSystem.h"
#include "RenderGraphBuilder.h"
#include "Engine.h"
#include "Loaders/glTFSceneLoader.h"
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
#include "RenderGraphCaptureUIView.h"
#include "UIElements/ApplicationUI.h"
#include "Shadows/CascadedShadowMapsViewRenderSystem.h"
#include "ParticipatingMedia/ParticipatingMediaViewRenderSystem.h"

#if SPT_SHADERS_DEBUG_FEATURES
#include "Debug/ShaderDebugUtils.h"
#endif // SPT_SHADERS_DEBUG_FEATURES

namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(Sandbox, true)

SandboxRenderer::SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow)
	: m_window(std::move(owningWindow))
	, m_fovDegrees(80.f)
	, m_nearPlane(0.1f)
	, m_farPlane(50.f)
	, m_cameraSpeed(5.f)
	, m_renderScene(lib::MakeShared<rsc::RenderScene>())
	, m_wantsCaptureNextFrame(false)
{
	InitializeRenderScene();
}

SandboxRenderer::~SandboxRenderer()
{
	// explicit destroy view before scene
	m_renderView.reset();
}

void SandboxRenderer::Tick(Real32 deltaTime)
{
	SPT_PROFILER_FUNCTION();

	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::W))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * math::Vector3f::UnitX()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::S))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * -math::Vector3f::UnitX()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::D))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * math::Vector3f::UnitY()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::A))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * m_cameraSpeed * -math::Vector3f::UnitY()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::E))
	{
		m_renderView->Move(deltaTime * m_cameraSpeed * math::Vector3f::UnitZ());
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::Q))
	{
		m_renderView->Move(deltaTime * m_cameraSpeed * -math::Vector3f::UnitZ());
	}

	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::RightMouseButton))
	{
		const math::Vector2f mouseVel = inp::InputManager::Get().GetMouseMoveDelta().cast<Real32>();
		if (mouseVel.squaredNorm() > math::Utils::Square(5.f) * deltaTime)
		{
			const Real32 rotationSpeed = 0.0045f;
			const math::Vector2f rotationDelta = rotationSpeed * mouseVel;

			m_renderView->Rotate(math::AngleAxisf(rotationDelta.x(), math::Vector3f::UnitZ()));
			m_renderView->Rotate(math::AngleAxisf(rotationDelta.y(), m_renderView->GetRotation() * math::Vector3f::UnitY()));
		}
	}
}

void SandboxRenderer::RenderFrame()
{
	SPT_PROFILER_FUNCTION();

	UpdateSceneUITextureForView(*m_renderView);

	const lib::SharedRef<rdr::GPUStatisticsCollector> gpuStatisticsCollector = lib::MakeShared<rdr::GPUStatisticsCollector>();

	rg::RenderGraphBuilder graphBuilder;
	graphBuilder.BindGPUStatisticsCollector(gpuStatisticsCollector);

	rg::capture::RenderGraphCapturer capturer;

	if (m_wantsCaptureNextFrame)
	{
		capturer.Capture(graphBuilder);
		m_wantsCaptureNextFrame = false;
	}

	engn::GetRenderingFrame().AddOnGPUFinishedDelegate(engn::OnGPUFinished::Delegate::CreateLambda([ resolution = m_renderView->GetRenderingResolution(), gpuStatisticsCollector ](engn::FrameContext& context)
																								   {
																									   prf::GPUProfilerStatistics statistics;
																									   statistics.resolution      = resolution;
																									   statistics.frameStatistics = gpuStatisticsCollector->CollectStatistics();
																									   prf::Profiler::Get().SetGPUFrameStatistics(std::move(statistics));
																								   }));

	rsc::SceneRendererSettings rendererSettings;
	rendererSettings.outputFormat = m_window->GetRHI().GetFragmentFormat();

	rg::RGTextureViewHandle sceneRenderingResultTextureView;

	{
#if SPT_SHADERS_DEBUG_FEATURES
		gfx::dbg::ShaderDebugParameters shaderDebugParameters;
		shaderDebugParameters.mousePosition = m_mousePositionOnViewport;
		const gfx::dbg::ShaderDebugScope shaderDebugCommandsCollectingScope(graphBuilder, shaderDebugParameters);
#endif // SPT_SHADERS_DEBUG_FEATURES

		sceneRenderingResultTextureView = m_sceneRenderer.Render(graphBuilder, *m_renderScene, *m_renderView, rendererSettings);
	}

	SPT_CHECK(sceneRenderingResultTextureView.IsValid());

	const rg::RGTextureViewHandle sceneUItextureView = graphBuilder.AcquireExternalTextureView(m_sceneUITextureView);

	graphBuilder.CopyTexture(RG_DEBUG_NAME("Copy Scene Result Texture"),
							 sceneRenderingResultTextureView, math::Vector3i::Zero(),
							 sceneUItextureView, math::Vector3i::Zero(),
							 sceneRenderingResultTextureView->GetResolution());

	// prepare for UI pass
	graphBuilder.ReleaseTextureWithTransition(sceneUItextureView->GetTexture(), rhi::TextureTransition::FragmentReadOnly);

	graphBuilder.Execute();
	
	if (capturer.HasValidCapture())
	{
		using GPUFinishedDelegate = engn::OnGPUFinished::Delegate;

		GPUFinishedDelegate delegate = GPUFinishedDelegate::CreateLambda([rgCapture = lib::Ref(capturer.GetCapture())](engn::FrameContext& context)
																		 {
																			 scui::ViewDefinition viewDef("Render Graph Capture Viewer");
																			 viewDef.minimumSize = math::Vector2u(800, 600);
																			 lib::SharedRef<rg::capture::RenderGraphCaptureUIView> captureView = lib::MakeShared<rg::capture::RenderGraphCaptureUIView>(viewDef, std::move(rgCapture));
																			 scui::ApplicationUI::AddView(std::move(captureView));
																		 });

		engn::GetRenderingFrame().AddOnGPUFinishedDelegate(std::move(delegate));
	}
}

ui::TextureID SandboxRenderer::GetUITextureID() const
{
	return m_sceneUITextureView ? rdr::UIBackend::GetUITextureID(lib::Ref(m_sceneUITextureView)) : ui::TextureID{};
}

const lib::SharedPtr<rdr::Window>& SandboxRenderer::GetWindow() const
{
	return m_window;
}

rsc::SceneRenderer& SandboxRenderer::GetSceneRenderer()
{
	return m_sceneRenderer;
}

const lib::SharedPtr<rsc::RenderScene>& SandboxRenderer::GetRenderScene()
{
	return m_renderScene;
}

void SandboxRenderer::SetImageSize(const math::Vector2u& imageSize)
{
	math::Vector2u renderingResolution = imageSize;
	// we want to have some minimal resolution as image size may be sometimes 0 (for example when imgui layout is resetted)
	renderingResolution.x() = std::max<Uint32>(renderingResolution.x(), 2);
	renderingResolution.y() = std::max<Uint32>(renderingResolution.y(), 2);
	m_renderView->SetRenderingResolution(renderingResolution);

	const Uint32 windowWidth = m_window->GetSwapchainSize().x();
	const Real32 renderingFov = (renderingResolution.x() / static_cast<Real32>(windowWidth)) * m_fovDegrees;
	const Real32 renderingFovRadians = math::Utils::DegreesToRadians(renderingFov);
	const Real32 reneringAspect = static_cast<Real32>(renderingResolution.x()) / static_cast<Real32>(renderingResolution.y());

	m_renderView->SetPerspectiveProjection(renderingFovRadians, reneringAspect, m_nearPlane, m_farPlane);
}

math::Vector2u SandboxRenderer::GetDisplayTextureResolution() const
{
	return m_sceneUITexture->GetResolution().head<2>();
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

void SandboxRenderer::CreateRenderGraphCapture()
{
	m_wantsCaptureNextFrame = true;
}

void SandboxRenderer::InitializeRenderScene()
{
	m_renderView = lib::MakeShared<rsc::RenderView>(*m_renderScene);
	m_renderView->AddRenderStages(rsc::ERenderStage::ForwardRendererStages);
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		m_renderView->AddRenderStages(rsc::ERenderStage::RayTracingRenderStages);
	}
	m_renderView->SetRenderingResolution(math::Vector2u(1920, 1080));
	m_renderView->SetPerspectiveProjection(math::Utils::DegreesToRadians(m_fovDegrees), 1920.f / 1080.f, m_nearPlane, m_farPlane);
	m_renderView->SetAntiAliasingMode(rsc::EAntiAliasingMode::TemporalAA);

	rsc::ShadowCascadesParams cascadesParams;
	cascadesParams.shadowsTechnique = rsc::EShadowMappingTechnique::VSM;
	m_renderView->AddRenderSystem<rsc::CascadedShadowMapsViewRenderSystem>(cascadesParams);
	m_renderView->AddRenderSystem<rsc::ParticipatingMediaViewRenderSystem>();


	rsc::RenderSceneEntityHandle viewEntity = m_renderView->GetViewEntity();
	
	if (lib::SharedPtr<rdr::Texture> lensDirtTexture = gfx::TextureLoader::LoadTexture(engn::Paths::Combine(engn::Paths::GetContentPath(), "Camera/LensDirt.jpeg"), lib::Flags(rhi::ETextureUsage::SampledTexture, rhi::ETextureUsage::TransferDest)))
	{
		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange = rhi::TextureSubresourceRange(rhi::ETextureAspect::Color);

		rsc::CameraLensSettingsComponent cameraLensSettingsComponent;
		cameraLensSettingsComponent.lensDirtTexture = lensDirtTexture->CreateView(RENDERER_RESOURCE_NAME("Lens Dirt View"), viewDef);
		viewEntity.emplace<rsc::CameraLensSettingsComponent>(cameraLensSettingsComponent);
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

	const lib::HashedString scenePath = engn::Engine::Get().GetCmdLineArgs().GetValue("-Scene");
	if (scenePath.IsValid())
	{
		const lib::String finalPath = engn::Paths::Combine(engn::Paths::GetContentPath(), scenePath.ToString());
		rsc::glTFLoader::LoadScene(*m_renderScene, finalPath);

		{
			const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
			rsc::PointLightData pointLightData;
			pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
			pointLightData.luminousPower = 3200.f;
			pointLightData.location = math::Vector3f(2.6f, 8.30f, 1.55f);
			pointLightData.radius = 5.f;
			lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
		}

		{
			const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
			rsc::PointLightData pointLightData;
			pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
			pointLightData.luminousPower = 3200.f;
			pointLightData.location = math::Vector3f(-3.9f, -8.30f, 4.45f);
			pointLightData.radius = 5.f;
			lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
		}

		{
			const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
			rsc::PointLightData pointLightData;
			pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
			pointLightData.luminousPower = 3200.f;
			pointLightData.location = math::Vector3f(-3.8f, 8.30f, 1.55f);
			pointLightData.radius = 5.f;
			lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
		}
		
		{
			const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
			rsc::PointLightData pointLightData;
			pointLightData.color = math::Vector3f(1.0f, 0.7333f, 0.451f);
			pointLightData.luminousPower = 3200.f;
			pointLightData.location = math::Vector3f(-3.8f, -9.30f, 1.55f);
			pointLightData.radius = 5.f;
			lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
		}

		{
			const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene->CreateEntity();
			rsc::DirectionalLightData directionalLightData;
			directionalLightData.color			= math::Vector3f(0.9569f, 0.9137f, 0.6078f);
			directionalLightData.illuminance	= 95000.f;
			directionalLightData.direction		= math::Vector3f(0.5f, 0.3f, -1.9f).normalized();
			//directionalLightData.direction		= math::Vector3f(0.5f, 0.3f, 0.1f).normalized();
			directionalLightData.lightConeAngle = 0.0046f;
			lightSceneEntity.emplace<rsc::DirectionalLightData>(directionalLightData);
		}
	}
}

void SandboxRenderer::UpdateSceneUITextureForView(const rsc::RenderView& renderView)
{
	const math::Vector2u& renderingResolution = renderView.GetRenderingResolution();
	if (!m_sceneUITexture || (m_sceneUITexture->GetResolution().head<2>().array() != renderingResolution.head<2>().array()).any())
	{
		rhi::TextureDefinition textureDef;
		textureDef.resolution	= renderingResolution;
		textureDef.usage		= lib::Flags(rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::SampledTexture);
		textureDef.format		= m_window->GetRHI().GetFragmentFormat();

		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange.aspect = rhi::ETextureAspect::Color;

		m_sceneUITexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("DisplayTexture"), textureDef, rhi::EMemoryUsage::GPUOnly);
		m_sceneUITextureView = m_sceneUITexture->CreateView(RENDERER_RESOURCE_NAME("DisplayTextureView"), viewDef);
	}
}

} // spt::ed
