#include "ProfilerUIView.h"
#include "ImGui/SculptorImGui.h"
#include "Profiler.h"
#include "ImGui/DockBuilder.h"

namespace spt::prf
{

ProfilerUIView::ProfilerUIView(const scui::ViewDefinition& definition)
	: Super(definition)
	, m_profilerPanelName(CreateUniqueName("Profiler"))
{ }

void ProfilerUIView::BuildDefaultLayout(ImGuiID dockspaceID)
{
	Super::BuildDefaultLayout(dockspaceID);

	ui::Build(dockspaceID, ui::DockWindow(m_profilerPanelName));
}

void ProfilerUIView::DrawUI()
{
	Super::DrawUI();
	
	ImGui::SetNextWindowClass(&scui::CurrentViewBuildingContext::GetCurrentViewContentClass());

	ImGui::Begin(m_profilerPanelName.GetData());

	ImGui::Separator();

	ImGui::Text("Profiler");

	if(ImGui::Button("Reset Scope Metrics"))
	{
		Profiler::Get().ResetScopeMetrics();
	}

	ImGui::PlotLines("Frame Time",
					 [](void* data, int idx)
					 {
						 const Real32 frameTime = prf::Profiler::Get().GetFrameTime(static_cast<SizeType>(idx));
						 const Real32 frameTimeMS = frameTime * 1000.f;
						 return frameTimeMS;
					 },
					 nullptr,
					 static_cast<int>(prf::Profiler::Get().GetCollectedFrameTimesNum()),
					 0, nullptr,
					 0.f, 50.f);

	const Real32 averageFrameTime = prf::Profiler::Get().GetAverageFrameTime();
	const Real32 averageFrameTimeMS = averageFrameTime * 1000.f;
	ImGui::Text("Average Frame Time: %fms", averageFrameTimeMS);
	ImGui::Text("Average FPS: %f", 1000.f / averageFrameTimeMS);

	ImGui::Separator();

	DrawGPUProfilerUI();

	ImGui::End();
}

void ProfilerUIView::DrawGPUProfilerUI()
{
	SPT_PROFILER_FUNCTION();

	if (ImGui::CollapsingHeader("GPU Stats"))
	{
		prf::Profiler::Get().FlushNewGPUFrameStatistics();

		const GPUProfilerStatistics& gpuFrameStatistics = prf::Profiler::Get().GetGPUFrameStatistics();

		if (gpuFrameStatistics.IsValid())
		{
			DrawGPUScopeStatistics(gpuFrameStatistics);
		}
	}
}

void ProfilerUIView::DrawGPUScopeStatistics(const GPUProfilerStatistics& profilerStats)
{
	ImGui::Text("Resolution: %d x %d", profilerStats.resolution.x(), profilerStats.resolution.y());

	DrawGPUScopeStatistics(profilerStats.frameStatistics, profilerStats.GetGPUFrameDuration());
}

void ProfilerUIView::DrawGPUScopeStatistics(const rdr::GPUStatisticsScopeData& scopeStats, rdr::GPUDurationMs frameDuration)
{
	ImGui::PushID(static_cast<int>(scopeStats.name.GetKey()));

	const Real32 scopeIndentVal = 16.f;

	const Real32 width = ImGui::GetColumnWidth();
	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, width * 0.8f);
	{
		ImGui::Text(scopeStats.name.GetData());
		ImGui::NextColumn();
		ImGui::Text("%fms", scopeStats.GetDurationInMs());
		ImGui::NextColumn();

		if (scopeStats.inputAsseblyVertices.has_value())
		{
			ImGui::Text("IA Vertices");
			ImGui::NextColumn();
			ImGui::Text("%u", scopeStats.inputAsseblyVertices.value());
			ImGui::NextColumn();
		}

		if (scopeStats.inputAsseblyPrimitives.has_value())
		{
			ImGui::Text("IA Primitives");
			ImGui::NextColumn();
			ImGui::Text("%u", scopeStats.inputAsseblyPrimitives.value());
			ImGui::NextColumn();
		}

		if (scopeStats.vertexShaderInvocations.has_value())
		{
			ImGui::Text("VS Invocations");
			ImGui::NextColumn();
			ImGui::Text("%u", scopeStats.vertexShaderInvocations.value());
			ImGui::NextColumn();
		}

		if (scopeStats.fragmentShaderInvocations.has_value())
		{
			ImGui::Text("FS Invocations");
			ImGui::NextColumn();
			ImGui::Text("%u", scopeStats.fragmentShaderInvocations.value());
			ImGui::NextColumn();
		}

		if (scopeStats.computeShaderInvocations.has_value())
		{
			ImGui::Text("CS Invocations");
			ImGui::NextColumn();
			ImGui::Text("%u", scopeStats.computeShaderInvocations.value());
			ImGui::NextColumn();
		}
	}
	ImGui::EndColumns();

	const rdr::GPUDurationMs framePercent = frameDuration > 0.0 ? scopeStats.GetDurationInMs() / frameDuration : 0.0;
	ImGui::ProgressBar(static_cast<Real32>(framePercent));
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text(scopeStats.name.GetData());
		ImGui::Text("%fms", scopeStats.GetDurationInMs());
		ImGui::EndTooltip();
	}

	{
		if (ImGui::CollapsingHeader("Stats"))
		{
			const ScopeMetrics metrics = Profiler::Get().GetScopeMetrics(scopeStats.name);
			ImGui::Text("Invocations Num: %d", metrics.invocationsNum);

			const rdr::GPUDurationNs totalDurationNs   = metrics.durationSum;
			const rdr::GPUDurationMs totalDurationMs   = totalDurationNs / 1000000.0;
			const rdr::GPUDurationMs averageDurationMs = totalDurationMs / metrics.invocationsNum;

			ImGui::Text("Total Time: %f ms",   totalDurationMs);
			ImGui::Text("Average Time: %f ms", averageDurationMs);
		}

		if (!scopeStats.children.empty() && ImGui::CollapsingHeader("Children"))
		{
			ImGui::Indent(scopeIndentVal);
			for (const rdr::GPUStatisticsScopeData& scope : scopeStats.children)
			{
				DrawGPUScopeStatistics(scope, frameDuration);
			}
			ImGui::Unindent(scopeIndentVal);
		}
	}

	ImGui::Separator();

	ImGui::PopID();
}

} // spt::prf
