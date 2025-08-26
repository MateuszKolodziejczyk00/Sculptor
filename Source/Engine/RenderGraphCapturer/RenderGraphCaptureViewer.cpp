#include "RenderGraphCaptureViewer.h"
#include "RenderGraphCaptureTypes.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
#include "UIUtils.h"
#include "ImGui/DockBuilder.h"
#include "TextureInspector/TextureInspector.h"
#include "TextureInspector/BufferInspector.h"
#include "Utility/Templates/Overload.h"


namespace spt::rg::capture
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGNodeCaptureViewer ===========================================================================

RGNodeCaptureViewer::RGNodeCaptureViewer(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture, const CapturedPass& node)
	: Super(definition)
	, m_nodeDetailsPanelName(CreateUniqueName(std::format("Node Details")))
	, m_nodePropertiesPanelName(CreateUniqueName(std::format("Node Properties")))
	, m_capturedPass(node)
	, m_capture(std::move(capture))
{
}

void RGNodeCaptureViewer::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID,
			 ui::Split(ui::ESplit::Vertical, 0.75f, 
					   ui::DockPrimitive(m_inspectorsStack),
					   ui::Split(ui::ESplit::Horizontal, 0.75f,
						   ui::DockWindow(m_nodeDetailsPanelName),
						   ui::DockWindow(m_nodePropertiesPanelName))));
}

void RGNodeCaptureViewer::DrawUI()
{
	Super::DrawUI();

	DrawNodeDetails(m_capturedPass);

	DrawNodeProperties(m_capturedPass);
}

ImGuiWindowFlags RGNodeCaptureViewer::GetWindowFlags() const
{
	return Super::GetWindowFlags()
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse;
}

void RGNodeCaptureViewer::OpenTextureCapture(const TextureInspectParams& inspectParams)
{
	scui::ViewDefinition nodeViewDefinition;
	nodeViewDefinition.name = inspectParams.texture->name;
	const lib::HashedString& viewName = AddChild(lib::MakeShared<TextureInspector>(nodeViewDefinition, m_capture, inspectParams));
	m_inspectorsStack.Push(viewName);
}

void RGNodeCaptureViewer::OpenTextureCapture(const CapturedTextureBinding& textureBinding)
{
	TextureInspectParams inspectParams;
	inspectParams.texture       = textureBinding.textureVersion->owningTexture;
	inspectParams.mipIdx        = textureBinding.baseMipLevel;
	inspectParams.arrayLayerIdx = textureBinding.baseArrayLayer;
	inspectParams.versionIdx    = textureBinding.textureVersion->versionIdx;
	OpenTextureCapture(inspectParams);
}

void RGNodeCaptureViewer::OpenBufferCapture(const CapturedBufferBinding& bufferBinding)
{
	scui::ViewDefinition nodeViewDefinition;
	nodeViewDefinition.name = bufferBinding.bufferVersion->owningBuffer->name;
	const lib::HashedString& viewName = AddChild(lib::MakeShared<BufferInspector>(nodeViewDefinition, *this, bufferBinding));
	m_inspectorsStack.Push(viewName);
}

void RGNodeCaptureViewer::DrawNodeDetails(const CapturedPass& pass)
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodeDetailsPanelName.GetData());

	ImGui::Text("Node: %s", pass.name.GetData());

	ImGui::Separator();

	const math::Vector2f contentSize = ui::UIUtils::GetWindowContentSize();

	ImGui::Columns(2);

	if (ImGui::CollapsingHeader("Input Textures"))
	{
		for (const CapturedTextureBinding& binding : pass.textures)
		{
			if (!binding.writable)
			{
				const lib::HashedString name = binding.textureVersion->owningTexture->name;
				if (ImGui::Button(name.GetData(), ImVec2(contentSize.x() * 0.5f, 26.f)))
				{
					OpenTextureCapture(binding);
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("Output Textures"))
	{
		for (const CapturedTextureBinding& binding : pass.textures)
		{
			if (binding.writable)
			{
				const lib::HashedString name = binding.textureVersion->owningTexture->name;
				if (ImGui::Button(name.GetData(), ImVec2(contentSize.x() * 0.5f, 26.f)))
				{
					OpenTextureCapture(binding);
				}
			}
		}
	}

	ImGui::NextColumn();
	
	if (ImGui::CollapsingHeader("Buffers"))
	{
		for (const CapturedBufferBinding& binding : pass.buffers)
		{
			const lib::HashedString name = binding.bufferVersion->owningBuffer->name;
			if (ImGui::Button(name.GetData(), ImVec2(contentSize.x() * 0.5f, 26.f)))
			{
				OpenBufferCapture(binding);
			}
		}
	}

	ImGui::EndColumns();

	ImGui::End();
}

void RGNodeCaptureViewer::DrawNodeProperties(const CapturedPass& node)
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodePropertiesPanelName.GetData());

	std::visit(lib::Overload
			   {
				   [this](const RGCapturedComputeProperties& properties) { DrawNodeComputeProperties(properties); },
				   [](const RGCapturedNullProperties& properties) { ImGui::Text("No properties were captured"); }
			   },
			   node.execProps);

	ImGui::End();
}

void RGNodeCaptureViewer::DrawNodeComputeProperties(const RGCapturedComputeProperties& properties)
{
	if (ImGui::CollapsingHeader("Pipeline State Statistics"))
	{
		const auto displayStatisticValue = [](rhi::PipelineStatisticValue value)
			{
				std::visit(lib::Overload
						   {
							   [](Bool value) { ImGui::Text("%s", value ? "true" : "false"); },
							   [](Uint64 value) { ImGui::Text("%llu", value); },
							   [](Int64 value) { ImGui::Text("%ll", value); },
							   [](Real64 value) { ImGui::Text("%f", static_cast<Real32>(value)); }
						   },
						   value);
			};

		ImGui::Columns(2);

		for (const rhi::PipelineStatistic& stat : properties.pipelineStatistics.statistics)
		{
			ImGui::Text(stat.name.c_str());
			if(ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text(stat.description.c_str());
				ImGui::EndTooltip();
			}

			ImGui::NextColumn();

			displayStatisticValue(stat.value);

			ImGui::NextColumn();
		}

		ImGui::EndColumns();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderGraphCaptureViewer ======================================================================

RenderGraphCaptureViewer::RenderGraphCaptureViewer(const scui::ViewDefinition& definition, lib::SharedRef<RGCapture> capture)
	: Super(definition)
	, m_nodesListPanelName(CreateUniqueName("Nodes List"))
	, m_nodesListFilter(CreateUniqueName("Nodes Filter"))
	, m_capture(std::move(capture))
{
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

	m_nodesListFilter.Draw();

	ImGui::Dummy(ImVec2(0.f, 30.f));

	ImGui::BeginChild("Nodes", ImVec2{}, true);

	const math::Vector2f contentSize = ui::UIUtils::GetWindowContentSize();

	for (Uint32 passIdx = 0u; passIdx < capture.passes.size(); ++passIdx)
	{
		const lib::UniquePtr<CapturedPass>& pass = capture.passes[passIdx];

		if (m_nodesListFilter.IsMatching(pass->name.GetView()))
		{
			if (ImGui::Button(std::format("{}: {}", passIdx, pass->name.GetData()).c_str(), ImVec2(contentSize.x(), 26.f)))
			{
				scui::ViewDefinition nodeViewDefinition;
				nodeViewDefinition.name = pass->name;
				const lib::HashedString& childName = AddChild(lib::MakeShared<RGNodeCaptureViewer>(nodeViewDefinition, m_capture, *pass));
				m_nodeDetailsDockStack.Push(childName);
			}

			ImGui::Spacing();
		}
	}

	ImGui::EndChild();

	ImGui::End();
}

} // spt::rg::capture
