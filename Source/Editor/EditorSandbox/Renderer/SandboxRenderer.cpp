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
#include "StaticMeshes/StaticMeshPrimitivesSystem.h"
#include "StaticMeshes/StaticMeshesRenderSystem.h"
#include "Lights/LightsRenderSystem.h"
#include "Shadows/ShadowMapsManagerSystem.h"
#include "SceneRenderer/SceneRenderer.h"
#include "InputManager.h"
#include "Transfers/TransfersManager.h"
#include "Lights/LightTypes.h"
#include "RayTracing/RayTracingSceneSystem.h"
#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"
#include "EngineFrame.h"

namespace spt::ed
{

SPT_DEFINE_LOG_CATEGORY(Sandbox, true)

SandboxRenderer::SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow)
	: m_window(std::move(owningWindow))
	, m_fovDegrees(80.f)
	, m_nearPlane(0.1f)
	, m_farPlane(50.f)
	, m_cameraSpeed(5.f)
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

lib::SharedPtr<rdr::Semaphore> SandboxRenderer::RenderFrame()
{
	SPT_PROFILER_FUNCTION();

	js::JobWithResult createFinishSemaphoreJob = js::Launch(SPT_GENERIC_JOB_NAME, []() -> lib::SharedRef<rdr::Semaphore>
															{
																const rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
																return rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);
															});

	js::Job flushPendingBufferUploads = js::Launch(SPT_GENERIC_JOB_NAME, []()
												   {
													   return gfx::FlushPendingUploads();
												   });

	UpdateSceneUITextureForView(*m_renderView);

	rg::RenderGraphBuilder graphBuilder;

	const rg::RGTextureViewHandle sceneRenderingResultTextureView = m_sceneRenderer.Render(graphBuilder, m_renderScene, *m_renderView);
	SPT_CHECK(sceneRenderingResultTextureView.IsValid());

	const rg::RGTextureViewHandle sceneUItextureView = graphBuilder.AcquireExternalTextureView(m_sceneUITextureView);

	graphBuilder.CopyTexture(RG_DEBUG_NAME("Copy Scene Result Texture"),
							 sceneRenderingResultTextureView, math::Vector3i::Zero(),
							 sceneUItextureView, math::Vector3i::Zero(),
							 sceneRenderingResultTextureView->GetResolution());

	// prepare for UI pass
	graphBuilder.ReleaseTextureWithTransition(sceneUItextureView->GetTexture(), rhi::TextureTransition::FragmentReadOnly);

	rdr::SemaphoresArray waitSemaphores;
#if !WITH_NSIGHT_CRASH_FIX
	// Wait for previous frame as we're reusing resources
	waitSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), engn::GetGPUFrame().GetFrameIdx(), rhi::EPipelineStage::ALL_COMMANDS);
#endif // !WITH_NSIGHT_CRASH_FIX
	
	flushPendingBufferUploads.Wait();
	gfx::TransfersManager::WaitForTransfersFinished(waitSemaphores);

	const lib::SharedRef<rdr::Semaphore> finishSemaphore = createFinishSemaphoreJob.Await();
	rdr::SemaphoresArray signalSemaphores;
	signalSemaphores.AddBinarySemaphore(finishSemaphore, rhi::EPipelineStage::ALL_COMMANDS);

	graphBuilder.Execute(waitSemaphores, signalSemaphores);

	return finishSemaphore.ToSharedPtr();
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

rsc::RenderView& SandboxRenderer::GetRenderView() const
{
	return *m_renderView;
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

void SandboxRenderer::InitializeRenderScene()
{
	m_renderView = lib::MakeShared<rsc::RenderView>(m_renderScene);
	m_renderView->AddRenderStages(lib::Flags(rsc::ERenderStage::DepthPrepass, rsc::ERenderStage::MotionAndDepth, rsc::ERenderStage::ForwardOpaque, rsc::ERenderStage::HDRResolve));
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		m_renderView->AddRenderStages(rsc::ERenderStage::DirectionalLightsShadowMasks);
	}
	m_renderView->SetRenderingResolution(math::Vector2u(1920, 1080));
	m_renderView->SetPerspectiveProjection(math::Utils::DegreesToRadians(m_fovDegrees), 1920.f / 1080.f, m_nearPlane, m_farPlane);

	m_renderScene.AddPrimitivesSystem<rsc::StaticMeshPrimitivesSystem>();
	m_renderScene.AddPrimitivesSystem<rsc::ShadowMapsManagerSystem>(m_renderView);
	if (rdr::Renderer::IsRayTracingEnabled())
	{
		m_renderScene.AddPrimitivesSystem<rsc::RayTracingSceneSystem>();
	}
	m_renderScene.AddRenderSystem<rsc::StaticMeshesRenderSystem>();
	m_renderScene.AddRenderSystem<rsc::LightsRenderSystem>();

	const lib::HashedString scenePath = engn::Engine::Get().GetCmdLineArgs().GetValue("-Scene");
	if (scenePath.IsValid())
	{
		const lib::String finalPath = engn::Paths::Combine(engn::Paths::GetContentPath(), scenePath.ToString());
		rsc::glTFLoader::LoadScene(m_renderScene, finalPath);

		for (Int32 x = -1; x <= 1; ++x)
		{
			for(Int32 y = -3; y <= 3; ++y)
			{
				for (Int32 z = 0; z <= 1; ++z)
				{
					const Real32 random = lib::rnd::Random<Real32>();

					const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene.CreateEntity();
					rsc::PointLightData pointLightData;
					pointLightData.color = math::Vector3f::Random() * 0.4f + math::Vector3f::Constant(0.6f);
					pointLightData.intensity = std::clamp(1.f - random, 8.f, 80.f);
					pointLightData.location = math::Vector3f(x * 3.1f, y * 2.6f, 0.9f + z * 4.6f);
					pointLightData.radius = 4.f;

					// bloom test
					if (x == -1 && y == 3 && z == 0)
					{
						pointLightData.intensity *= 12.f;
					}
					
					lightSceneEntity.emplace<rsc::PointLightData>(pointLightData);
				}
			}
		}

		{
			const rsc::RenderSceneEntityHandle lightSceneEntity = m_renderScene.CreateEntity();
			rsc::DirectionalLightData directionalLightData;
			directionalLightData.color			= math::Vector3f::Ones();
			directionalLightData.intensity		= 5.f;
			directionalLightData.direction		= math::Vector3f(0.5f, 1.f, -2.7f).normalized();
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
		textureDef.resolution = math::Vector3u(renderingResolution.x(), renderingResolution.y(), 1);
		textureDef.usage = lib::Flags(rhi::ETextureUsage::TransferDest, rhi::ETextureUsage::SampledTexture);

		rhi::TextureViewDefinition viewDef;
		viewDef.subresourceRange.aspect = rhi::ETextureAspect::Color;

		m_sceneUITexture = rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME("DisplayTexture"), textureDef, rhi::RHIAllocationInfo());
		m_sceneUITextureView = m_sceneUITexture->CreateView(RENDERER_RESOURCE_NAME("DisplayTextureView"), viewDef);
	}
}

} // spt::ed
