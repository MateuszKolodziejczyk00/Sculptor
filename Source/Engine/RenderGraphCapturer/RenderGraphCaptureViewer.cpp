#include "RenderGraphCaptureViewer.h"
#include "RenderGraphCaptureTypes.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
#include "UIUtils.h"
#include "ImGui/DockBuilder.h"
#include "TextureInspector/TextureInspector.h"


namespace spt::rg::capture
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGNodeCaptureViewer ===========================================================================

RGNodeCaptureViewer::RGNodeCaptureViewer(const scui::ViewDefinition& definition, const RGNodeCapture& node)
	: Super(definition)
	, m_nodeDetailsPanelName(CreateUniqueName(std::format("Node Details")))
	, m_capturedNode(node)
{
}

void RGNodeCaptureViewer::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			 ui::Split(ui::ESplit::Vertical, 0.75f, 
					   ui::DockPrimitive(m_textureInspectorsStack),
					   ui::DockWindow(m_nodeDetailsPanelName)));
}

void RGNodeCaptureViewer::DrawUI()
{
	Super::DrawUI();

	DrawNodeDetails(m_capturedNode);
}

ImGuiWindowFlags RGNodeCaptureViewer::GetWindowFlags() const
{
	return Super::GetWindowFlags()
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse;
}

void RGNodeCaptureViewer::DrawNodeDetails(const RGNodeCapture& node)
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodeDetailsPanelName.GetData());

	ImGui::Text("Node: %s", node.name.GetData());

	ImGui::Separator();

	const math::Vector2f contentSize = ui::UIUtils::GetWindowContentSize();
	
	if (ImGui::CollapsingHeader("Output Textures"))
	{
		for (const RGTextureCapture& outputTexture : node.outputTextures)
		{
			if (ImGui::Button(outputTexture.textureView->GetRHI().GetName().GetData(), ImVec2(contentSize.x(), 26.f)))
			{
				OpenTextureCapture(outputTexture);
			}
		}
	}

	ImGui::End();
}

void RGNodeCaptureViewer::OpenTextureCapture(const RGTextureCapture& textureCapture)
{
	scui::ViewDefinition nodeViewDefinition;
	nodeViewDefinition.name = textureCapture.textureView->GetRHI().GetName();
	const lib::HashedString& viewName = AddChild(lib::MakeShared<TextureInspector>(nodeViewDefinition, textureCapture));
	m_textureInspectorsStack.Push(viewName);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderGraphCaptureViewer ======================================================================

RenderGraphCaptureViewer::RenderGraphCaptureViewer(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture)
	: Super(definition)
	, m_nodesListPanelName(CreateUniqueName("Nodes List"))
	, m_nodesListFilterName(CreateUniqueName("Nodes Filter"))
	, m_capture(std::move(capture))
{
	m_nodesListFilter[0] = '\0';
}

void RenderGraphCaptureViewer::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			  ui::Split(ui::ESplit::Horizontal, 0.25f,
						ui::DockWindow(m_nodesListPanelName),
						ui::DockPrimitive(m_nodeDetailsDockStack)));
}

void RenderGraphCaptureViewer::DrawUI()
{
	SPT_PROFILER_FUNCTION();

	Super::DrawUI();

	DrawNodesList(GetCapture());
}

void RenderGraphCaptureViewer::DrawNodesList(const RGCapture& capture)
{
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodesListPanelName.GetData());

	if (ImGui::CollapsingHeader("Filter"))
	{
		ImGui::Columns(2);
		ImGui::InputText(m_nodesListFilterName.GetData(), m_nodesListFilter, SPT_ARRAY_SIZE(m_nodesListFilter));
		ImGui::NextColumn();
		if (ImGui::Button("Clear"))
		{
			m_nodesListFilter[0] = '\0';
		}

		ImGui::EndColumns();
	}

	ImGui::Dummy(ImVec2(0.f, 30.f));

	ImGui::BeginChild("Nodes", ImVec2{}, true);

	const lib::StringView filterText = m_nodesListFilter;

	const math::Vector2f contentSize = ui::UIUtils::GetWindowContentSize();

	for (const RGNodeCapture& node : capture.nodes)
	{
		if (filterText.empty() || node.name.GetView().find(filterText) != lib::StringView::npos)
		{
			if (ImGui::Button(node.name.GetData(), ImVec2(contentSize.x(), 26.f)))
			{
				scui::ViewDefinition nodeViewDefinition;
				nodeViewDefinition.name = node.name;
				const lib::HashedString& childName = AddChild(lib::MakeShared<RGNodeCaptureViewer>(nodeViewDefinition, node));
				m_nodeDetailsDockStack.Push(childName);
			}

			ImGui::Spacing();
		}
	}

	ImGui::EndChild();

	ImGui::End();
}

} // spt::rg::capture
