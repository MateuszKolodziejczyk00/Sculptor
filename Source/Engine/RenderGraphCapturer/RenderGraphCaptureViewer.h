#pragma once

#include "SculptorCoreTypes.h"
#include "EngineFrame.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

struct RGCapture;
struct RGNodeCapture;


class RenderGraphCaptureViewer : private engn::EngineTickable<engn::EFrameState::Simulation, RenderGraphCaptureViewer>
{
public:

	explicit RenderGraphCaptureViewer(lib::SharedRef<RGCapture> capture);

	void Tick(Real32 deltaTime);

	const RGCapture& GetCapture() const;

	void SelectNode(const RGNodeCapture* node);
	const RGNodeCapture* GetSelectedNode() const;

	void SelectTexture(lib::SharedPtr<rdr::TextureView> texture);
	const lib::SharedPtr<rdr::TextureView>& GetSelectedTexture() const;

	Real32 GetZoom() const;

private:

	lib::SharedRef<RGCapture> m_capture;

	const RGNodeCapture* m_selectedNode;

	lib::SharedPtr<rdr::TextureView> m_selectedTexture;

	Real32 m_zoom;
};

} // spt::rg::capture