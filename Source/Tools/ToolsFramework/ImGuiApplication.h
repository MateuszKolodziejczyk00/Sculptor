#pragma once

#include "SculptorCoreTypes.h"
#include "ScopedEngine.h"
#include "UIContext.h"
#include "Allocators/MemoryArena.h"
#include "RenderGraphResourcesPool.h"


namespace spt::rdr
{
class Window;
class GPUWorkload;
} // spt::rdr


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rg::capture
{
class RGCaptureSourceContext;
struct RGCapture;
} // spt::rg::capture


namespace spt::tools
{

class ImGuiApplication
{
public:

	explicit ImGuiApplication(lib::StringView name, math::Vector2u windowResolution = math::Vector2u(1600u, 900u));
	virtual ~ImGuiApplication();

	ImGuiApplication(const ImGuiApplication&) = delete;
	ImGuiApplication& operator=(const ImGuiApplication&) = delete;

	void Run();

protected:

	virtual void DrawUI() = 0;

	virtual void RenderFrame(rg::RenderGraphBuilder& graphBuilder) {}

	void RequestRenderGraphCapture();

private:

	void InitializeUIFonts();

	void ExecuteFrame();

	void ExecuteRenderGraph();

	void RenderUI();

	void OpenCaptureViewer(lib::SharedPtr<rg::capture::RGCapture> capture);

	lib::SharedPtr<rdr::Window> m_window;

	ui::UIContext m_uiContext;

	lib::String m_imGuiConfigPath;

	lib::MemoryArena m_frameArena;

	rg::RenderGraphResourcesPool m_renderGraphResourcesPool;

	lib::SharedPtr<rdr::GPUWorkload> m_lastFrameWorkload;

	lib::SharedPtr<rg::capture::RGCaptureSourceContext> m_captureSourceContext;

	Bool m_wantsRenderGraphCapture = false;

	lib::SharedPtr<rg::capture::RGCapture> m_pendingCapture;
};


template<typename TApplication>
int RunApplication()
{
	ScopedEngine engine;

	TApplication application;
	application.Run();

	return 0;
}

} // spt::tools
