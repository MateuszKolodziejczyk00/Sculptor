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
#include "BufferUtilities.h"
#include "StaticMeshes/StaticMeshPrimitivesSystem.h"
#include "StaticMeshes/StaticMeshesRenderSystem.h"
#include "SceneRenderer/SceneRenderer.h"
#include "InputManager.h"

namespace spt::ed
{

SandboxRenderer::SandboxRenderer(lib::SharedPtr<rdr::Window> owningWindow)
	: m_window(std::move(owningWindow))
	, m_fovDegrees(80.f)
	, m_nearPlane(1.f)
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

	const Real32 cameraSpeed = 5.f;

	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::W))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * cameraSpeed * math::Vector3f::UnitX()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::S))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * cameraSpeed * -math::Vector3f::UnitX()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::A))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * cameraSpeed * math::Vector3f::UnitY()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::D))
	{
		m_renderView->Move(m_renderView->GetRotation() * (deltaTime * cameraSpeed * -math::Vector3f::UnitY()));
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::E))
	{
		m_renderView->Move(deltaTime * cameraSpeed * math::Vector3f::UnitZ());
	}
	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::Q))
	{
		m_renderView->Move(deltaTime * cameraSpeed * -math::Vector3f::UnitZ());
	}

	if (inp::InputManager::Get().IsKeyPressed(inp::EKey::RightMouseButton))
	{
		const math::Vector2f mouseVel = inp::InputManager::Get().GetMouseMoveDelta().cast<Real32>();
		if (mouseVel.squaredNorm() > math::Utils::Square(5.f) * deltaTime)
		{
			const Real32 rotationSpeed = 0.0045f;
			const math::Vector2f rotationDelta = rotationSpeed * mouseVel;

			m_renderView->Rotate(math::AngleAxisf(-rotationDelta.x(), math::Vector3f::UnitZ()));
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
																return rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FinishCommandsSemaphore"), semaphoreDef);
															});

	js::JobWithResult flushPendingBufferUploads = js::Launch(SPT_GENERIC_JOB_NAME, []() -> lib::SharedPtr<rdr::Semaphore>
															 {
																 return gfx::FlushPendingUploads();
															 });

	UpdateSceneUITextureForView(*m_renderView);

	rg::RenderGraphBuilder graphBuilder;

	rsc::SceneRenderer sceneRenderer;
	const rg::RGTextureViewHandle sceneRenderingResultTextureView = sceneRenderer.Render(graphBuilder, m_renderScene, *m_renderView);
	SPT_CHECK(sceneRenderingResultTextureView.IsValid());

	const rg::RGTextureViewHandle sceneUItextureView = graphBuilder.AcquireExternalTextureView(m_sceneUITextureView);

	graphBuilder.CopyTexture(RG_DEBUG_NAME("Copy Scene Result Texture"),
							 sceneRenderingResultTextureView, math::Vector3i::Zero(),
							 sceneUItextureView, math::Vector3i::Zero(),
							 sceneRenderingResultTextureView->GetResolution());

	// prepare for UI pass
	graphBuilder.ReleaseTextureWithTransition(sceneUItextureView->GetTexture(), rhi::TextureTransition::FragmentReadOnly);

	rdr::SemaphoresArray waitSemaphores;
	// Wait for previous frame as we're reusing resources
	waitSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx() - 1, rhi::EPipelineStage::ALL_COMMANDS);
	
	const lib::SharedPtr<rdr::Semaphore> finishPendingUploadsSemaphore = flushPendingBufferUploads.Await();
	if (finishPendingUploadsSemaphore)
	{
		waitSemaphores.AddBinarySemaphore(finishPendingUploadsSemaphore, rhi::EPipelineStage::ALL_COMMANDS);
	}

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

	m_renderView->SetPerspectiveProjection(renderingFovRadians, reneringAspect, m_nearPlane);
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

void SandboxRenderer::InitializeRenderScene()
{
	m_renderScene.AddPrimitivesSystem<rsc::StaticMeshPrimitivesSystem>();
	m_renderScene.AddRenderSystem<rsc::StaticMeshesRenderSystem>();

	const lib::HashedString scenePath = engn::Engine::Get().GetCmdLineArgs().GetValue("-Scene");
	if (scenePath.IsValid())
	{
		const lib::String finalPath = engn::Paths::Combine(engn::Paths::GetContentPath(), scenePath.ToString());
		rsc::glTFLoader::LoadScene(m_renderScene, finalPath);
	}

	m_renderView = std::make_unique<rsc::RenderView>(m_renderScene);
	m_renderView->AddRenderStages(lib::Flags(rsc::ERenderStage::ForwardOpaque, rsc::ERenderStage::DepthPrepass));
	m_renderView->SetRenderingResolution(math::Vector2u(1920, 1080));
	m_renderView->SetPerspectiveProjection(math::Utils::DegreesToRadians(m_fovDegrees), 1920.f / 1080.f, m_nearPlane);
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
