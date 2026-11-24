#include "RenderGraphCaptureViewer.h"
#include "RenderGraphCaptureTypes.h"
#include "Types/Texture.h"
#include "Types/UIBackend.h"
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

// View panel names contain per-instance IDs, so saved table settings would only pile up in the ImGui config
constexpr ImGuiTableFlags listTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;

constexpr lib::StaticArray<const char*, 4u> textureColumns = { "Name", "Resolution", "Format", "Mips" };
constexpr lib::StaticArray<const char*, 3u> bufferColumns  = { "Name", "Type", "Size" };

lib::String FormatRange(Uint32 base, Uint32 num)
{
	return num == 1u ? std::to_string(base) : std::format("{}-{}", base, base + num - 1u);
}

lib::String FormatBytes(Uint64 bytes)
{
	if (bytes < 1024u)
	{
		return std::format("{} B", bytes);
	}

	return bytes < 1024u * 1024u
		? std::format("{:.1f} KB", static_cast<Real64>(bytes) / 1024.0)
		: std::format("{:.1f} MB", static_cast<Real64>(bytes) / (1024.0 * 1024.0));
}

const char* GetProducingPassName(const CapturedPass* producingPass)
{
	return producingPass ? producingPass->name.GetData() : "External";
}

// Scopes tree interleaves RG diagnostics scopes with node scopes (and capture copy nodes), all in execution order.
// Node scope is the next scope with matching name at the depth of node's diagnostics record
void AssignPassesGPUStatistics(lib::Span<const rdr::GPUStatisticsScopeData> scopes, Uint32 depth, const lib::DynamicArray<lib::UniquePtr<CapturedPass>>& passes, INOUT SizeType& passIdx)
{
	for (const rdr::GPUStatisticsScopeData& scope : scopes)
	{
		if (passIdx < passes.size() && passes[passIdx]->diagnosticsScopeDepth == depth && passes[passIdx]->name == scope.name)
		{
			passes[passIdx++]->gpuStatistics = &scope;
		}
		else
		{
			AssignPassesGPUStatistics(scope.children, depth + 1u, passes, INOUT passIdx);
		}
	}
}

void ResolveGPUStatistics(RGCapture& capture)
{
	capture.gpuStatistics = std::make_unique<rdr::GPUStatisticsData>();
	capture.gpuStatisticsCollector->CollectStatistics(OUT *capture.gpuStatistics);
	capture.gpuStatisticsCollector.reset();

	if (const rdr::GPUStatisticsScopeData* frameScope = capture.gpuStatistics->fullScope)
	{
		SizeType passIdx = 0u;
		AssignPassesGPUStatistics(frameScope->children, 0u, capture.passes, INOUT passIdx);
		SPT_CHECK_MSG(passIdx == capture.passes.size(), "Failed to match GPU statistics for pass {}", capture.passes[passIdx]->name.GetData());
	}
}

template<typename TBinding, typename TDrawRow>
void DrawBindingsSection(const char* title, const lib::DynamicArray<TBinding>& bindings, Bool writable, lib::Span<const char* const> columns, TDrawRow&& drawRow)
{
	const auto hasMatchingAccess = [writable](const TBinding& binding) { return binding.writable == writable; };

	const SizeType bindingsNum = static_cast<SizeType>(std::count_if(bindings.cbegin(), bindings.cend(), hasMatchingAccess));

	const Bool isOpen = ImGui::CollapsingHeader(std::format("{} ({})###{}", title, bindingsNum, title).c_str(), ImGuiTreeNodeFlags_DefaultOpen);

	if (isOpen && bindingsNum > 0u && ImGui::BeginTable(title, static_cast<Int32>(columns.size()), listTableFlags))
	{
		for (SizeType columnIdx = 0u; columnIdx < columns.size(); ++columnIdx)
		{
			ImGui::TableSetupColumn(columns[columnIdx], columnIdx == 0u ? ImGuiTableColumnFlags_WidthStretch : ImGuiTableColumnFlags_WidthFixed);
		}
		ImGui::TableHeadersRow();

		for (const TBinding& binding : bindings)
		{
			if (hasMatchingAccess(binding))
			{
				ImGui::PushID(&binding);
				ImGui::TableNextRow();
				drawRow(binding);
				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}
}

Bool DrawMatchedResourceRow(const lib::String& label, const char* resourceType, Bool writable)
{
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(1);
	ImGui::Indent();
	ImGui::TextColored(ImGui::GetStyleColorVec4(writable ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled), "%s %s", writable ? "Out" : "In", resourceType);
	ImGui::SameLine();
	const Bool clicked = ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
	ImGui::Unindent();
	return clicked;
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

void RGNodeCaptureViewer::DrawNodeGPUStatistics(const rdr::GPUStatisticsScopeData& statistics)
{
	if (!ImGui::CollapsingHeader("GPU Statistics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (ImGui::BeginTable("##GPUStatistics", 2, priv::listTableFlags))
	{
		ImGui::TableSetupColumn("Statistic", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		const auto drawRow = [](const char* name, const lib::String& value)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(name);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(value.c_str());
		};

		const auto drawOptionalRow = [&drawRow](const char* name, const auto& value)
		{
			if (value.has_value())
			{
				drawRow(name, std::to_string(*value));
			}
		};

		drawRow("Duration", std::format("{:.3f} ms", statistics.GetDurationInMs()));
		drawOptionalRow("Task Shader Invocations", statistics.taskShaderInvocations);
		drawOptionalRow("Mesh Shader Invocations", statistics.meshShaderInvocations);
		drawOptionalRow("Fragment Shader Invocations", statistics.fragmentShaderInvocations);
		drawOptionalRow("Compute Shader Invocations", statistics.computeShaderInvocations);

		if (statistics.transientMemoryUsage.has_value())
		{
			drawRow("Transient Memory", priv::FormatBytes(*statistics.transientMemoryUsage));
		}

		for (const rdr::GPUStatisticsScopeData& subpass : statistics.children)
		{
			drawRow(subpass.name.GetData(), std::format("{:.3f} ms", subpass.GetDurationInMs()));
		}

		ImGui::EndTable();
	}
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

	ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", pass.name.GetData());
	ImGui::SameLine();
	ImGui::TextDisabled("%zu textures, %zu buffers", pass.textures.size(), pass.buffers.size());

	ImGui::Separator();

	const auto drawTextureRow = [this](const CapturedTextureBinding& binding)
	{
		const CapturedTexture::Version& version = *binding.textureVersion;
		const CapturedTexture& texture          = *version.owningTexture;
		const math::Vector3u& resolution        = texture.definition.resolution.AsVector();

		ImGui::TableNextColumn();
		if (ImGui::Selectable(texture.name.GetData(), false, ImGuiSelectableFlags_SpanAllColumns))
		{
			OpenTextureCapture(binding);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s\nVersion: %u\nArray layers: %s\nProduced by: %s",
							  texture.name.GetData(),
							  version.versionIdx,
							  priv::FormatRange(binding.baseArrayLayer, binding.arrayLayersNum).c_str(),
							  priv::GetProducingPassName(version.producingPass));
		}

		ImGui::TableNextColumn();
		if (resolution.z() > 1u)
		{
			ImGui::TextDisabled("%ux%ux%u", resolution.x(), resolution.y(), resolution.z());
		}
		else
		{
			ImGui::TextDisabled("%ux%u", resolution.x(), resolution.y());
		}

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", rhi::GetFormatName(texture.definition.format).c_str());

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", priv::FormatRange(binding.baseMipLevel, binding.mipLevelsNum).c_str());
	};

	const auto drawBufferRow = [this](const CapturedBufferBinding& binding)
	{
		const CapturedBuffer::Version& version = *binding.bufferVersion;
		const CapturedBuffer& buffer           = *version.owningBuffer;

		ImGui::TableNextColumn();
		if (ImGui::Selectable(buffer.name.GetData(), false, ImGuiSelectableFlags_SpanAllColumns))
		{
			OpenBufferCapture(binding);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s\nVersion: %u\nOffset: %llu\nElements: %u\nProduced by: %s",
							  buffer.name.GetData(),
							  version.versionIdx,
							  binding.offset,
							  binding.elementsNum,
							  priv::GetProducingPassName(version.producingPass));
		}

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", binding.structTypeName.IsValid() ? binding.structTypeName.GetData() : "-");

		ImGui::TableNextColumn();
		ImGui::TextDisabled("%s", priv::FormatBytes(binding.size).c_str());
	};

	if (ImGui::BeginTable("##Bindings", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableNextColumn();
		priv::DrawBindingsSection("Input Textures", pass.textures, false, priv::textureColumns, drawTextureRow);
		priv::DrawBindingsSection("Output Textures", pass.textures, true, priv::textureColumns, drawTextureRow);

		ImGui::TableNextColumn();
		priv::DrawBindingsSection("Input Buffers", pass.buffers, false, priv::bufferColumns, drawBufferRow);
		priv::DrawBindingsSection("Output Buffers", pass.buffers, true, priv::bufferColumns, drawBufferRow);

		ImGui::EndTable();
	}

	ImGui::End();
}

void RGNodeCaptureViewer::DrawNodeProperties(const CapturedPass& node)
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());
	ImGui::Begin(m_nodePropertiesPanelName.GetData());

	if (node.gpuStatistics)
	{
		DrawNodeGPUStatistics(*node.gpuStatistics);
	}

	std::visit(lib::Overload
			   {
				   [this](const RGCapturedComputeProperties& properties) { DrawNodeComputeProperties(properties); },
				   [](const RGCapturedNullProperties& properties) { ImGui::TextDisabled("No properties were captured"); }
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
							   [](Int64 value) { ImGui::Text("%lld", value); },
							   [](Real64 value) { ImGui::Text("%f", static_cast<Real32>(value)); }
						   },
						   value);
			};

		if (ImGui::BeginTable("##Statistics", 2, priv::listTableFlags))
		{
			ImGui::TableSetupColumn("Statistic", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableHeadersRow();

			for (const rhi::PipelineStatistic& stat : properties.pipelineStatistics.statistics)
			{
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(stat.name.c_str());
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", stat.description.c_str());
				}

				ImGui::TableNextColumn();
				displayStatisticValue(stat.value);
			}

			ImGui::EndTable();
		}
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
	if (m_capture->gpuStatisticsCollector)
	{
		// Viewer is always created after captured graph finished on GPU
		priv::ResolveGPUStatistics(*m_capture);
	}

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

	m_nodesListFilter.Draw("pass | resource  (Ctrl+F)");

	const auto openPassCapture = [this](Uint32 passIdx, const CapturedPass& pass)
	{
		scui::ViewDefinition nodeViewDefinition;
		nodeViewDefinition.name = pass.name;

		lib::SharedRef<RGNodeCaptureViewer> nodeViewer = lib::MakeShared<RGNodeCaptureViewer>(nodeViewDefinition, m_capture, pass);
		const lib::HashedString& childName = AddChild(nodeViewer);
		m_nodeDetailsDockStack.Push(childName);
		m_lastOpenedPassIdx = passIdx;

		return nodeViewer;
	};

	const auto isTextureMatching = [this](const CapturedTextureBinding& binding) { return m_nodesListFilter.IsMatching(priv::BuildTextureBindingLabel(binding), 1u); };
	const auto isBufferMatching  = [this](const CapturedBufferBinding& binding) { return m_nodesListFilter.IsMatching(priv::BuildBufferBindingLabel(binding), 1u); };

	const Bool filterResources = m_nodesListFilter.GetElementsNum() > 1u;

	if (ImGui::BeginTable("##Passes", 5, priv::listTableFlags | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Tex", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Buf", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		for (Uint32 passIdx = 0u; passIdx < capture.passes.size(); ++passIdx)
		{
			const CapturedPass& pass = *capture.passes[passIdx];

			const Bool isPassVisible = m_nodesListFilter.IsMatching(pass.name.GetView())
				&& (!filterResources
					|| std::any_of(pass.textures.cbegin(), pass.textures.cend(), isTextureMatching)
					|| std::any_of(pass.buffers.cbegin(), pass.buffers.cend(), isBufferMatching));

			if (!isPassVisible)
			{
				continue;
			}

			ImGui::PushID(static_cast<Int32>(passIdx));

			ImGui::TableNextRow();

			ImGui::TableNextColumn();
			ImGui::TextDisabled("%u", passIdx);

			ImGui::TableNextColumn();
			if (ImGui::Selectable(pass.name.GetData(), passIdx == m_lastOpenedPassIdx, ImGuiSelectableFlags_SpanAllColumns))
			{
				openPassCapture(passIdx, pass);
			}

			ImGui::TableNextColumn();
			if (pass.gpuStatistics)
			{
				ImGui::TextDisabled("%.3f", pass.gpuStatistics->GetDurationInMs());
			}

			ImGui::TableNextColumn();
			ImGui::TextDisabled("%zu", pass.textures.size());

			ImGui::TableNextColumn();
			ImGui::TextDisabled("%zu", pass.buffers.size());

			if (filterResources)
			{
				for (const CapturedTextureBinding& binding : pass.textures)
				{
					if (isTextureMatching(binding))
					{
						ImGui::PushID(&binding);
						if (priv::DrawMatchedResourceRow(priv::BuildTextureBindingLabel(binding), "Tex", binding.writable))
						{
							openPassCapture(passIdx, pass)->OpenTextureCapture(binding);
						}
						ImGui::PopID();
					}
				}

				for (const CapturedBufferBinding& binding : pass.buffers)
				{
					if (isBufferMatching(binding))
					{
						ImGui::PushID(&binding);
						if (priv::DrawMatchedResourceRow(priv::BuildBufferBindingLabel(binding), "Buf", binding.writable))
						{
							openPassCapture(passIdx, pass)->OpenBufferCapture(binding);
						}
						ImGui::PopID();
					}
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

} // spt::rg::capture
