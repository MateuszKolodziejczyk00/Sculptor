#pragma once

#include "RenderGraphCaptuererMacros.h"
#include "UILayers/UIView.h"
#include "UITypes.h"
#include "ImGui/DockStack.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rg::capture
{

struct RGCapture;
struct RGNodeCapture;
struct RGTextureCapture;


class RGNodeCaptureViewer : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	RGNodeCaptureViewer(const scui::ViewDefinition& definition, const RGNodeCapture& node);

	// Begin UIView overrides
	virtual void             BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void             DrawUI() override;
	virtual ImGuiWindowFlags GetWindowFlags() const override;
	// End UIView overrides

private:

	void DrawNodeDetails(const RGNodeCapture& node);

	void OpenTextureCapture(const RGTextureCapture& textureCapture);

	lib::HashedString m_nodeDetailsPanelName;

	ui::DockStack m_textureInspectorsStack;

	const RGNodeCapture& m_capturedNode;
};


class RENDER_GRAPH_CAPTURER_API RenderGraphCaptureViewer : public scui::UIView
{
protected:

	using Super = scui::UIView;

public:

	RenderGraphCaptureViewer(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture);

	// Begin UIView overrides
	virtual void BuildDefaultLayout(ImGuiID dockspaceID) override;
	virtual void DrawUI() override;
	// End UIView overrides

	const RGCapture& GetCapture() const { return *m_capture; }

private:

	void DrawNodesList(const RGCapture& capture);

	lib::HashedString m_nodesListPanelName;
	lib::HashedString m_nodesListFilterName;

	ui::DockStack m_nodeDetailsDockStack;

	char m_nodesListFilter[64];

	lib::SharedRef<RGCapture> m_capture;
};

} // spt::rg::capture
