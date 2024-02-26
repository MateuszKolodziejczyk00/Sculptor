#pragma once

#include "Application/Application.h"
#include "SculptorCoreTypes.h"
#include "Types/Window.h"
#include "Types/UIBackend.h"
#include "EngineFrame.h"



namespace spt::rdr
{
class GPUWorkload;
class RenderContext;
} // spt::rdr


namespace spt::ed
{

class SandboxRenderer;
class EditorFrameContext;


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

	void ExecuteFrame(EditorFrameContext& frame);

	void UpdateUI(EditorFrameContext& frame);

	std::pair<lib::SharedRef<rdr::RenderContext>, lib::SharedRef<rdr::GPUWorkload>> RecordUICommands(rhi::EFragmentFormat rtFormat) const;

	rdr::SwapchainTextureHandle AcquireSwapchainTexture(EditorFrameContext& frame);
	
	void RenderFrame(EditorFrameContext& frame, rdr::SwapchainTextureHandle swapchainTextureHandle, const lib::SharedRef<rdr::GPUWorkload>& recordedUIWorkload);

	lib::SharedPtr<rdr::Window> m_window;

	ui::UIContext uiContext;
};

} // spt::ed
