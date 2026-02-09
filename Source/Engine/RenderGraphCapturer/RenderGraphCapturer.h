#pragma once

#include "RenderGraphCapturerMacros.h"
#include "RenderGraphCaptureTypes.h"

namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rg::capture
{

namespace impl
{
class RGCapturerDecorator;
} // impl


struct CaptureParameters
{
	Bool captureTextures = true;
	Bool captureBuffers  = true;
};


class RENDER_GRAPH_CAPTURER_API RenderGraphCapturer
{
public:

	explicit RenderGraphCapturer(const CaptureParameters& params, const RGCaptureSourceInfo& captureSource);

	void Capture(RenderGraphBuilder& graphBuilder);

	Bool HasValidCapture() const;

	lib::SharedPtr<RGCapture> GetCapture() const;

private:
	
	lib::SharedPtr<impl::RGCapturerDecorator> m_debugDecorator;

	CaptureParameters   m_captureParams;
	RGCaptureSourceInfo m_captureSource;
};

} // spt::rg::capture
