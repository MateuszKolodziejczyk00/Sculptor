#include "SculptorEdApplication.h"
#include "Window/WindowImpl.h"
#include "Profiler.h"


namespace spt::ed
{

SculptorEdApplication::SculptorEdApplication()
{
}

void SculptorEdApplication::OnInit()
{
	Super::OnInit();

	m_window = std::make_shared<window::GLFWWindow>("SculptorEd", math::Vector2i(1920, 1080));
}

void SculptorEdApplication::OnRun()
{
	Super::OnRun();

	while (!m_window->ShouldClose())
	{
		SPT_PROFILE_FRAME("Frame");
		m_window->Update(0.1f);
	}
}

void SculptorEdApplication::OnShutdown()
{
	Super::OnShutdown();

	m_window.reset();
}

}
