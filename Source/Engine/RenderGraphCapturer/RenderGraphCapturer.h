#pragma once

#include "RenderGraphCaptuererMacros.h"
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

	RenderGraphCapturer();

	void Capture(RenderGraphBuilder& graphBuilder);

	Bool HasValidCapture() const;

	lib::SharedPtr<RGCapture> GetCapture() const;

private:
	
	lib::SharedPtr<impl::RGCapturerDecorator> m_debugDecorator;
};

} // spt::rg::capture