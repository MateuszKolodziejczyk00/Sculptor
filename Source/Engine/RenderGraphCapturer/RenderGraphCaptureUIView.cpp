#include "RenderGraphCaptureUIView.h"
#include "RenderGraphCaptureTypes.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
#include "UIUtils.h"
#include "ImGui/DockBuilder.h"


namespace spt::rg::capture
{

RenderGraphCaptureUIView::RenderGraphCaptureUIView(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture)
	: Super(definition)
	, m_captureViewer(std::move(capture))
	, m_nodesListPanelName(CreateUniqueName("Nodes List"))
	, m_nodeDetailsPanelName(CreateUniqueName("Node Details"))
	, m_textureViewPanelName(CreateUniqueName("Texture View"))
{ }

void RenderGraphCaptureUIView::BuildDefaultLayout(ImGuiID dockspaceID) const
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			 ui::Split(ui::ESplit::Horizontal, 0.25f,
					   ui::DockWindow(m_nodesListPanelName),
					   ui::Split(ui::ESplit::Horizontal, 0.33f,
								 ui::DockWindow(m_nodeDetailsPanelName),
								 ui::DockWindow(m_textureViewPanelName))));
}

void RenderGraphCaptureUIView::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	Super::DrawUI();

	DrawNodesList(m_captureViewer.GetCapture());

	if (m_captureViewer.GetSelectedNode())
	{
		DrawNodeDetails(*m_captureViewer.GetSelectedNode());

		if (m_captureViewer.GetSelectedTexture())
		{
			DrawTexture(lib::Ref(m_captureViewer.GetSelectedTexture()));
		}
	}
}

void RenderGraphCaptureUIView::DrawNodesList(const RGCapture& capture)
{
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodesListPanelName.GetData());

	const math::Vector2f contentSize = ui::UIUtils::GetWindowContentSize();

	for (const RGNodeCapture& node : capture.nodes)
	{
		Bool isNodeSelected = &node == m_captureViewer.GetSelectedNode();
		if (ImGui::Selectable(node.name.GetData(), &isNodeSelected))
		{
			m_captureViewer.SelectNode(&node);
		}
	}


	ImGui::End();
}

void RenderGraphCaptureUIView::DrawNodeDetails(const RGNodeCapture& node)
{
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodeDetailsPanelName.GetData());

	ImGui::Text("Node: %s", node.name.GetData());

	ImGui::Separator();
	
	for (const RGTextureCapture& outputTexture : node.outputTextures)
	{
		Bool isTextureSelected = outputTexture.textureView == m_captureViewer.GetSelectedTexture();
		if (ImGui::Selectable(outputTexture.textureView->GetRHI().GetName().GetData(), &isTextureSelected))
		{
			m_captureViewer.SelectTexture(outputTexture.textureView);
		}
	}

	ImGui::End();
}

void RenderGraphCaptureUIView::DrawTexture(const lib::SharedRef<rdr::TextureView>& texture)
{
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_textureViewPanelName.GetData(), nullptr, ImGuiWindowFlags_HorizontalScrollbar);
	
	const ui::TextureID textureID = rdr::UIBackend::GetUITextureID(texture, rhi::ESamplerFilterType::Nearest);

	const math::Vector2f textureSize = texture->GetRHI().GetResolution2D().cast<Real32>() * m_captureViewer.GetZoom();

	ImGui::Image(textureID, textureSize, ImVec2{}, ImVec2{ 1.f, 1.f });

	ImGui::End();
}

} // spt::rg::capture
