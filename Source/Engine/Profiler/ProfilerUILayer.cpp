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
			prf::Profiler::Get().StopCapture();
			prf::Profiler::Get().SaveCapture();
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

	ImGui::End();
}

} // spt::prf
