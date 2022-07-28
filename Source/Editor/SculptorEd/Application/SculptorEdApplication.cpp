#include "SculptorEdApplication.h"
#include "Renderer.h"
#include "RendererBuilder.h"
#include "Profiler.h"
#include "Timer/TickingTimer.h"
#include "Types/Semaphore.h"
#include "Types/Texture.h"


namespace spt::ed
{

SculptorEdApplication::SculptorEdApplication()
{
}

void SculptorEdApplication::OnInit()
{
	Super::OnInit();

	renderer::Renderer::Initialize();

	m_window = renderer::RendererBuilder::CreateWindow("SculptorEd", math::Vector2u(1920, 1080));
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	lib::TickingTimer timer;

	while (!m_window->ShouldClose())
	{
		SPT_PROFILE_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();

		renderer::Renderer::BeginFrame();

		m_window->BeginFrame();

		rhi::SemaphoreDefinition semaphoreDef(rhi::ESemaphoreType::Binary);
		const lib::SharedPtr<renderer::Semaphore> acquireSemaphore = renderer::RendererBuilder::CreateSemaphore(RENDERER_RESOURCE_NAME("AcquireSemaphore"), semaphoreDef);

		const lib::SharedPtr<renderer::Texture> swapchainTexture = m_window->AcquireNextSwapchainTexture(acquireSemaphore);

		m_window->PresentTexture({ acquireSemaphore });

		m_window->Update(deltaTime);

		renderer::Renderer::EndFrame();
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	m_window.reset();

	renderer::Renderer::Shutdown();
}

}
