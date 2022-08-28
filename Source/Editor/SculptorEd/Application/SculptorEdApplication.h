#pragma once

#include "Application/Application.h"
#include "SculptorCoreTypes.h"
#include "Types/Window.h"
#include "Types/UIBackend.h"

namespace spt::ed
{

class SculptorEdApplication : public lib::Application
{
protected:

	using Super = lib::Application;

public:

	SculptorEdApplication();

protected:

	// Begin lib::Application overrides
	virtual void OnInit(int argc, char** argv) override;
	virtual void OnRun() override;
	virtual void OnShutdown() override;
	// End lib::Application overrides

private:

	void RenderFrame();

	lib::SharedPtr<renderer::Window> m_window;
	lib::SharedPtr<renderer::UIBackend> uiBackend;
};

}
