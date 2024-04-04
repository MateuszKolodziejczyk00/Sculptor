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

class RENDER_GRAPH_CAPTURER_API RenderGraphCapturer
{
public:

	explicit RenderGraphCapturer(const RGCaptureSourceInfo& captureSource);

	void Capture(RenderGraphBuilder& graphBuilder);

	Bool HasValidCapture() const;

	lib::SharedPtr<RGCapture> GetCapture() const;

private:
	
	lib::SharedPtr<impl::RGCapturerDecorator> m_debugDecorator;

	RGCaptureSourceInfo m_captureSource;
};

} // spt::rg::capture