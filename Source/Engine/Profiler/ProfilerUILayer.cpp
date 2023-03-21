#include "ProfilerUILayer.h"
#include "ImGui/SculptorImGui.h"
#include "Profiler.h"
#include "UIElements/UIWindow.h"

namespace spt::prf
{

ProfilerUILayer::ProfilerUILayer(const scui::LayerDefinition& definition)
	: Super(definition)
{ }

void ProfilerUILayer::DrawUI()
{
	Super::DrawUI();
	
	ImGuiWindowClass windowsClass;
	windowsClass.ClassId = scui::CurrentWindowBuildingContext::GetCurrentWindowDockspaceID();

	ImGui::SetNextWindowClass(&windowsClass);

	ImGui::Begin("Profiler");

	ImGui::Separator();

	ImGui::Text("Profiler");

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

	ImGui::Text("Captures");

	if (prf::Profiler::Get().StartedCapture())
	{
		if (ImGui::Button("Stop Trace"))
		{
			prf::Profiler::Get().StopAndSaveCapture();
		}
	}
	else
	{
		if (ImGui::Button("Start Trace"))
		{
			prf::Profiler::Get().StartCapture();
		}
	}

	ImGui::Separator();

	DrawGPUProfilerUI();

	ImGui::End();
}

void ProfilerUILayer::DrawGPUProfilerUI()
{
	SPT_PROFILER_FUNCTION();

	if (ImGui::CollapsingHeader("GPU Stats"))
	{
		const rdr::GPUStatisticsScopeResult& gpuFrameStatistics = prf::Profiler::Get().GetGPUFrameStatistics();

		if (gpuFrameStatistics.IsValid())
		{
			DrawGPUScopeStatistics(gpuFrameStatistics, gpuFrameStatistics.durationInMs);
		}
	}
}

void ProfilerUILayer::DrawGPUScopeStatistics(const rdr::GPUStatisticsScopeResult& scopeStats, Real32 frameDuration)
{
	const Real32 scopeIndentVal = 16.f;

	const Real32 width = ImGui::GetColumnWidth();
	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, width * 0.8f);
	{
		ImGui::Text(scopeStats.name.GetData());
		ImGui::NextColumn();
		ImGui::Text("%fms", scopeStats.durationInMs);
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

	const Real32 framePercent = frameDuration > 0.f ? scopeStats.durationInMs / frameDuration : 0.f;
	ImGui::ProgressBar(framePercent);
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text(scopeStats.name.GetData());
		ImGui::Text("%fms", scopeStats.durationInMs);
		ImGui::EndTooltip();
	}

	{
		ImGui::Indent(scopeIndentVal);
		for (const rdr::GPUStatisticsScopeResult& scope : scopeStats.children)
		{
			DrawGPUScopeStatistics(scope, frameDuration);
		}
		ImGui::Unindent(scopeIndentVal);
	}

	ImGui::Separator();
}

} // spt::prf
