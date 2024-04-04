#include "RenderGraphCaptureSourceContext.h"


namespace spt::rg::capture
{

RGCaptureSourceContext::RGCaptureSourceContext()
{ }

void RGCaptureSourceContext::ExecuteOnSetupNewGraphBuilder(rg::RenderGraphBuilder& graphBuilder)
{
	m_onSetupNewGraphBuilder.Broadcast(graphBuilder);
}

lib::DelegateHandle RGCaptureSourceContext::AddOnSetupNewGraphBuilder(OnSetupNewGraph::Delegate&& delegate)
{
	return m_onSetupNewGraphBuilder.Add(std::move(delegate));
}

void RGCaptureSourceContext::RemoveOnSetupNewGraphBuilder(lib::DelegateHandle handle)
{
	m_onSetupNewGraphBuilder.Unbind(handle);
}

} // spt::rg::capture
