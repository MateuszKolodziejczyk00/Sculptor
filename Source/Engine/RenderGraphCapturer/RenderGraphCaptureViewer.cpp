#include "RenderGraphCaptureViewer.h"
#include "RenderGraphCaptureTypes.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
#include "UIUtils.h"
#include "ImGui/DockBuilder.h"
#include "TextureInspector/TextureInspector.h"
#include "TextureInspector/BufferInspector.h"
#include "Utility/Templates/Overload.h"
#include "JobSystem.h"


namespace spt::rg::capture
{

namespace priv
{

lib::String BuildTextureBindingLabel(const CapturedTextureBinding& binding)
{
	return binding.textureVersion ? binding.textureVersion->owningTexture->name.ToString() : lib::String("Unknown Texture");
}

lib::String BuildBufferBindingLabel(const CapturedBufferBinding& binding)
{
	lib::String label;

	if (binding.structTypeName.IsValid())
	{
		label = binding.structTypeName.ToString();
		label += " ";
	}

	if (binding.bufferVersion)
	{
		label += binding.bufferVersion->owningBuffer->name.ToString();
	}
	else
	{
		label += "Unknown Buffer";
	}

	return label;
}

} // priv

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
	FlushPendingInspectorOpen();

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
	if (!m_canOpenInspectors)
	{
		m_pendingInspectorOpen = inspectParams;
		return;
	}

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
	BufferInspectParams inspectParams;
	inspectParams.bufferVersion  = bufferBinding.bufferVersion;
	inspectParams.offset         = bufferBinding.offset;
	inspectParams.size           = bufferBinding.size;
	inspectParams.structTypeName = bufferBinding.structTypeName;
	OpenBufferCapture(inspectParams);
}

void RGNodeCaptureViewer::OpenBufferCapture(const BufferInspectParams& inspectParams)
{
	if (!m_canOpenInspectors)
	{
		m_pendingInspectorOpen = inspectParams;
		return;
	}

	scui::ViewDefinition nodeViewDefinition;
	nodeViewDefinition.name = inspectParams.bufferVersion->owningBuffer->name;
	const lib::HashedString& viewName = AddChild(lib::MakeShared<BufferInspector>(nodeViewDefinition, *this, inspectParams));
	m_inspectorsStack.Push(viewName);
}

void RGNodeCaptureViewer::FlushPendingInspectorOpen()
{
	m_canOpenInspectors = true;

	if (!m_pendingInspectorOpen.has_value())
	{
		return;
	}

	const PendingInspectorOpen pendingInspectorOpen = std::move(*m_pendingInspectorOpen);
	m_pendingInspectorOpen.reset();

	std::visit(lib::Overload
			   {
				   [this](const TextureInspectParams& inspectParams)
				   {
					   OpenTextureCapture(inspectParams);
				   },
				   [this](const BufferInspectParams& inspectParams)
				   {
					   OpenBufferCapture(inspectParams);
				   }
			   },
			   pendingInspectorOpen);
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

	if (ImGui::CollapsingHeader("Input Textures", ImGuiTreeNodeFlags_DefaultOpen))
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

	if (ImGui::CollapsingHeader("Output Textures", ImGuiTreeNodeFlags_DefaultOpen))
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
	
	if (ImGui::CollapsingHeader("Input Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const CapturedBufferBinding& binding : pass.buffers)
		{
			if(!binding.writable)
			{
				lib::String name = binding.structTypeName.ToString();
				name += " ";
				name += binding.bufferVersion->owningBuffer->name.ToString();

				if (ImGui::Button(name.c_str(), ImVec2(contentSize.x() * 0.5f, 26.f)))
				{
					OpenBufferCapture(binding);
				}
			}
		}
	}
	
	if (ImGui::CollapsingHeader("Output Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const CapturedBufferBinding& binding : pass.buffers)
		{
			if(binding.writable)
			{
				lib::String name = binding.structTypeName.ToString();
				name += " ";
				name += binding.bufferVersion->owningBuffer->name.ToString();

				if (ImGui::Button(name.c_str(), ImVec2(contentSize.x() * 0.5f, 26.f)))
				{
					OpenBufferCapture(binding);
				}
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
	if (ImGui::CollapsingHeader("Pipeline State Statistics", ImGuiTreeNodeFlags_DefaultOpen))
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

static void DownloadBufferVersion(lib::SharedRef<RGCapture> capture, CapturedBuffer& buffer, CapturedBuffer::Version& version)
{
	const SizeType bufferSize = version.downloadedBuffer->GetSize();
	const SizeType copySizePerJob = 16u * 1024u * 1024u;

	const SizeType jobsNum = (bufferSize + copySizePerJob - 1u) / copySizePerJob;

	version.pendingDownloadsNum = static_cast<Uint32>(jobsNum);
	version.bufferData.resize(bufferSize);

	for (Uint32 jobIdx = 0u; jobIdx < jobsNum; ++jobIdx)
	{
		const SizeType offset = jobIdx * copySizePerJob;
		const SizeType size = std::min(copySizePerJob, bufferSize - offset);
		js::Launch(SPT_GENERIC_JOB_NAME, [capture, &version, offset, size]()
				   {
					   SPT_PROFILER_SCOPE("Download Captured Buffer");
					   if (version.downloadedBuffer)
					   {
						   if (capture->wantsClose) // early out to free capture memory ASAP
						   {
							   return;
						   }

						   {
							   const rhi::RHIMappedByteBuffer mappedData(version.downloadedBuffer->GetRHI());
							   std::memcpy(version.bufferData.data() + offset, static_cast<Byte*>(mappedData.GetPtr()) + offset, size);
						   }

						   const Uint32 pendingDownloads = --version.pendingDownloadsNum;
						   if (pendingDownloads == 0u)
						   {
							   version.downloadedBuffer.reset();
						   }
					   }
				   },
				   js::JobDef().SetPriority(js::EJobPriority::Low));
	}
}

static void DownloadBufferVersions(lib::SharedRef<RGCapture> capture, CapturedBuffer& buffer)
{
	for (const lib::UniquePtr<CapturedBuffer::Version>& version : buffer.versions)
	{
		js::Launch(SPT_GENERIC_JOB_NAME, [capture, &buffer, version = version.get()]()
				   {
					   if (version->downloadedBuffer)
					   {
						   DownloadBufferVersion(capture, buffer, *version);
					   }
				   },
				   js::JobDef().SetPriority(js::EJobPriority::Low));
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
	js::Launch(SPT_GENERIC_JOB_NAME, [capture = m_capture]()
			   {
				   for (const lib::UniquePtr<CapturedBuffer>& buffer : capture->buffers)
				   {
					   DownloadBufferVersions(capture, *buffer);
				   }
			   });
}

RenderGraphCaptureViewer::~RenderGraphCaptureViewer()
{
	m_capture->wantsClose = true;
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
	const auto openPassCapture = [this](const CapturedPass& pass)
	{
		scui::ViewDefinition nodeViewDefinition;
		nodeViewDefinition.name = pass.name;

		lib::SharedRef<RGNodeCaptureViewer> nodeViewer = lib::MakeShared<RGNodeCaptureViewer>(nodeViewDefinition, m_capture, pass);
		const lib::HashedString& childName = AddChild(nodeViewer);
		m_nodeDetailsDockStack.Push(childName);

		return nodeViewer;
	};

	for (Uint32 passIdx = 0u; passIdx < capture.passes.size(); ++passIdx)
	{
		const lib::UniquePtr<CapturedPass>& pass = capture.passes[passIdx];

		if (m_nodesListFilter.IsMatching(pass->name.GetView()))
		{
			ImGui::PushID(static_cast<Int32>(passIdx));

			if (ImGui::Button(std::format("{}: {}", passIdx, pass->name.GetData()).c_str(), ImVec2(contentSize.x(), 26.f)))
			{
				openPassCapture(*pass);
			}

			if (m_nodesListFilter.GetElementsNum() > 1)
			{
				const float resourceButtonWidth = std::max(contentSize.x() - ImGui::GetStyle().IndentSpacing, 1.f);
				Bool hasFilteredResources = false;

				ImGui::Indent();

				ImGui::PushID("Textures");
				for (Uint32 textureIdx = 0u; textureIdx < pass->textures.size(); ++textureIdx)
				{
					const CapturedTextureBinding& binding = pass->textures[textureIdx];
					const lib::String label = priv::BuildTextureBindingLabel(binding);
					if (m_nodesListFilter.IsMatching(label, 1u))
					{
						hasFilteredResources = true;

						ImGui::PushID(static_cast<Int32>(textureIdx));
						const lib::String buttonLabel = std::format("{} Texture: {}", binding.writable ? "Output" : "Input", label);
						if (ImGui::Button(buttonLabel.c_str(), ImVec2(resourceButtonWidth, 24.f)))
						{
							openPassCapture(*pass)->OpenTextureCapture(binding);
						}
						ImGui::PopID();
					}
				}
				ImGui::PopID();

				ImGui::PushID("Buffers");
				for (Uint32 bufferIdx = 0u; bufferIdx < pass->buffers.size(); ++bufferIdx)
				{
					const CapturedBufferBinding& binding = pass->buffers[bufferIdx];
					const lib::String label = priv::BuildBufferBindingLabel(binding);
					if (m_nodesListFilter.IsMatching(label, 1u))
					{
						hasFilteredResources = true;

						ImGui::PushID(static_cast<Int32>(bufferIdx));
						const lib::String buttonLabel = std::format("{} Buffer: {}", binding.writable ? "Output" : "Input", label);
						if (ImGui::Button(buttonLabel.c_str(), ImVec2(resourceButtonWidth, 24.f)))
						{
							openPassCapture(*pass)->OpenBufferCapture(binding);
						}
						ImGui::PopID();
					}
				}
				ImGui::PopID();

				if (hasFilteredResources)
				{
					ImGui::Spacing();
				}

				ImGui::Unindent();
			}


			ImGui::Spacing();
			ImGui::PopID();
		}
	}

	ImGui::EndChild();

	ImGui::End();
}

} // spt::rg::capture
