#include "RenderGraphCaptureViewer.h"
#include "InputManager.h"


namespace spt::rg::capture
{

RenderGraphCaptureViewer::RenderGraphCaptureViewer(lib::SharedRef<RGCapture> capture)
	: m_capture(std::move(capture))
	, m_selectedNode(nullptr)
	, m_zoom(1.0f)
{ }

void RenderGraphCaptureViewer::Tick(Real32 deltaTime)
{
	SPT_PROFILER_FUNCTION();

	inp::InputManager& input = inp::InputManager::Get();

	m_zoom = std::clamp(m_zoom + input.GetScrollDelta() * 0.1f, 0.1f, 100.0f);
}

const RGCapture& RenderGraphCaptureViewer::GetCapture() const
{
	return *m_capture;
}

void RenderGraphCaptureViewer::SelectNode(const RGNodeCapture* node)
{
	m_selectedNode = node;
}

const RGNodeCapture* RenderGraphCaptureViewer::GetSelectedNode() const
{
	return m_selectedNode;
}

void RenderGraphCaptureViewer::SelectTexture(lib::SharedPtr<rdr::TextureView> texture)
{
	m_selectedTexture = std::move(texture);
}

const lib::SharedPtr<rdr::TextureView>& RenderGraphCaptureViewer::GetSelectedTexture() const
{
	return m_selectedTexture;
}

Real32 RenderGraphCaptureViewer::GetZoom() const
{
	return m_zoom;
}

} // spt::rg::capture
