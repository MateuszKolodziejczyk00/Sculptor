#pragma once

#include "RenderGraphCapturerMacros.h"
#include "SculptorCoreTypes.h"
#include "Delegates/MulticastDelegate.h"


namespace spt::rg
{

class RenderGraphBuilder;


namespace capture
{

class RENDER_GRAPH_CAPTURER_API RGCaptureSourceContext
{
public:

	using OnSetupNewGraph = lib::MulticastDelegate<void(rg::RenderGraphBuilder& /*graphBuilder*/)>;

	RGCaptureSourceContext();

	void ExecuteOnSetupNewGraphBuilder(rg::RenderGraphBuilder& graphBuilder);

	lib::DelegateHandle AddOnSetupNewGraphBuilder(OnSetupNewGraph::Delegate&& delegate);
	void RemoveOnSetupNewGraphBuilder(lib::DelegateHandle handle);


private:

	OnSetupNewGraph m_onSetupNewGraphBuilder;
};

} // capture

} // spt::rg