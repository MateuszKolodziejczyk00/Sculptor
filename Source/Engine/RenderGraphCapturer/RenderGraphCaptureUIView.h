#pragma once

#include "RenderGraphCaptuererMacros.h"
#include "UILayers/UIView.h"
#include "UITypes.h"
#include "RenderGraphCaptureViewer.h"


namespace spt::rg::capture
{

class RENDER_GRAPH_CAPTURER_API RenderGraphCaptureUIView : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	RenderGraphCaptureUIView(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture);

	// Begin UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) const override;
	virtual void DrawUI() override;
	// End UIView overrides

private:

	void DrawNodesList(const RGCapture& capture);

	void DrawNodeDetails(const RGNodeCapture& node);

	void DrawTexture(const lib::SharedRef<rdr::TextureView>& texture);

	RenderGraphCaptureViewer m_captureViewer;

	lib::HashedString m_nodesListPanelName;
	lib::HashedString m_nodeDetailsPanelName;
	lib::HashedString m_textureViewPanelName;
};

} // spt::rg::capture
