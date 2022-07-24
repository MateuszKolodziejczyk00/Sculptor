#include "SculptorEdApplication.h"
#include "Window/PlatformWindowImpl.h"
#include "Profiler.h"
#include "Timer/TickingTimer.h"


namespace spt::ed
{

SculptorEdApplication::SculptorEdApplication()
{
}

void SculptorEdApplication::OnInit()
{
	Super::OnInit();

	m_window = std::make_shared<platform::PlatformWindow>("SculptorEd", math::Vector2i(1920, 1080));
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	lib::TickingTimer timer;

	while (!m_window->ShouldClose())
	{
		SPT_PROFILE_FRAME("EditorFrame");

		const Real32 deltaTime = timer.Tick();

		m_window->Update(deltaTime);
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	m_window.reset();
}

}
