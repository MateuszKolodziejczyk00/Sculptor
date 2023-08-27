#pragma once

#include "Application/Application.h"
#include "SculptorCoreTypes.h"
#include "Types/Window.h"
#include "Types/UIBackend.h"
#include "EngineFrame.h"


namespace spt::ed
{

class SandboxRenderer;


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

	void RenderFrame(SandboxRenderer& renderer);

	void PreExecuteFrame();
	void ExecuteSimulationFrame(engn::FrameContext& context);
	void ExecuteRenderingFrame(engn::FrameContext& context);

	lib::SharedPtr<rdr::Window> m_window;
	lib::UniquePtr<SandboxRenderer> m_renderer;

	ui::UIContext uiContext;

	Bool isSwapchainValid;
};

}
