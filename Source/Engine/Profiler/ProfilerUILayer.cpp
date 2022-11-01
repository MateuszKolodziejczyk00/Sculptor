#include "ProfilerUILayer.h"
#include "ImGui/SculptorImGui.h"
#include "Profiler.h"

namespace spt::prf
{

ProfilerUILayer::ProfilerUILayer(const scui::LayerDefinition& definition)
	: Super(definition)
{ }

void ProfilerUILayer::DrawUI()
{
	Super::DrawUI();

	ImGui::Begin("Profiler");
	
	if (prf::Profiler::StartedCapture())
	{
		if (ImGui::Button("Stop Trace"))
		{
			prf::Profiler::StopCapture();
			prf::Profiler::SaveCapture();
		}
	}
	else
	{
		if (ImGui::Button("Start Trace"))
		{
			prf::Profiler::StartCapture();
		}
	}

	ImGui::End();
}

} // spt::prf
